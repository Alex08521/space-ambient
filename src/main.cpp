#include <cstring>
#include <atomic>
#include <sys/stat.h>
#include <pulse/simple.h>
#include <iostream>
#include <vector>
#include <signal.h>
#include <unistd.h>
#include <pulse/pulseaudio.h>
#include <pulse/def.h>
#include <vorbis/vorbisfile.h>
#include <cerrno>
#include <thread>
#include <string>
#include <random>
#include <algorithm>

#include <QGuiApplication>
#include <QString>
#include <KWindowSystem>
#include <KWindowInfo>
#include <NETWM>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusMessage>
#include <QFile>
#include <QTimer>

#include "genaudio.h"

std::vector<AudioData> audio_files;

std::atomic<int> keep_running = 1;
std::atomic<bool> is_playing = false;

int current_track = 0;

class AudioMonitor : public QObject {
    Q_OBJECT
public:
    AudioMonitor() {
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &AudioMonitor::checkAudioApplications);
        m_timer->start(300);
        
        QDBusConnection::sessionBus().connect(
            QStringLiteral("org.kde.KGlobalAccel"), 
            QStringLiteral("/components/mediacontrol"), 
            QStringLiteral("org.kde.kglobalaccel.Component"), 
            QStringLiteral("globalShortcutPressed"),
            this, 
            SLOT(checkAudioApplications())
        );
    }

private Q_SLOTS:
    void checkAudioApplications() { 
        checkPulseAudioStreams(getApplicationsWindowsVector());
    }
    
private:
    QTimer* m_timer;
    
    QString getProcessName(int pid) {
        if (pid <= 0) return QString();
        
        QFile file(QStringLiteral("/proc/%1/comm").arg(pid));
        if (file.open(QIODevice::ReadOnly)) {
            return QString::fromUtf8(file.readAll()).trimmed();
        }
        
        return QString();
    }

    std::vector<pid_t> getApplicationsWindowsVector(){
        QDBusInterface kwinInterface(
            QString::fromUtf8("org.kde.KWin"), 
            QString::fromUtf8("/KWin"), 
            QString::fromUtf8("org.kde.KWin"), 
            QDBusConnection::sessionBus(),
            nullptr
        );

        if (!kwinInterface.isValid()) {
            std::cerr << "Не удалось подключиться к KWin D-Bus интерфейсу";
        }

        QDBusMessage reply = kwinInterface.call(QString::fromUtf8("evaluateScript"), 
            QString::fromUtf8("const clients = workspace.windowList(); \
            let result = []; \
            for (let client of clients) { \
                result.push(client.pid); \
            } \
            result.join(';');"));
        
        QString resultString = reply.arguments().first().toString();
        QStringList windowEntries = resultString.split(QString::fromUtf8(";"), Qt::SkipEmptyParts);
        std::vector<pid_t> windows;

        for (const auto& entry : windowEntries) {
            bool okPid;
            pid_t pid = entry.toLongLong(&okPid);
                
            if (okPid) {
                windows.push_back(pid);
            }
        }
        
        return windows;
    }
    
    std::vector<QString> checkPulseAudioStreams(const std::vector<pid_t> cWindows) {
        pa_mainloop* ml = pa_mainloop_new();
        pa_mainloop_api* api = pa_mainloop_get_api(ml);
        pa_context* ctx = pa_context_new(api, "AudioMonitor");

        static const std::vector<pid_t>* pcWindows = &cWindows;
        
        pa_context_connect(ctx, nullptr, PA_CONTEXT_NOFLAGS, nullptr);
        
        struct Data {
            bool hasActiveStreams = false;
            std::vector<QString> audioAppsVector;
            pa_mainloop* mainloop;
        };
        
        Data data{false, {}, ml};
        
        pa_context_set_state_callback(ctx, [](pa_context* c, void* userdata) {
            Data* data = static_cast<Data*>(userdata);
            
            if (pa_context_get_state(c) == PA_CONTEXT_READY) {
                pa_operation* op = pa_context_get_sink_input_info_list(c, 
                    [](pa_context* c, const pa_sink_input_info* i, int eol, void* userdata) {
                        (void)c;
                        Data* data = static_cast<Data*>(userdata);
                        
                        if (eol) {
                            pa_mainloop_quit(data->mainloop, 0);
                            return;
                        }
                        
                        if (i && i->proplist) {
                            const char* appName = pa_proplist_gets(i->proplist, "application.name");
                            const char* process_id_str = pa_proplist_gets(i->proplist, PA_PROP_APPLICATION_PROCESS_ID);
                            pid_t pid = static_cast<pid_t>(process_id_str ? std::stoi(process_id_str) : 0);
                            if(std::find(pcWindows->begin(), pcWindows->end(), pid) != pcWindows->end())
                                data->audioAppsVector.push_back(QString::fromUtf8(appName));

                            if (appName && strcmp(appName, "Space Ambient Daemon") != 0) {
                                if (i->has_volume == true) {
                                    data->hasActiveStreams = true;
                                }
                            }
                        }
                    }, userdata);
                    
                if (op) pa_operation_unref(op);
                else pa_mainloop_quit(data->mainloop, 0);
            } else if (pa_context_get_state(c) == PA_CONTEXT_FAILED) {
                pa_mainloop_quit(data->mainloop, 0);
            }
        }, &data);
        
        int ret;
        pa_mainloop_run(ml, &ret);
        
        is_playing = !data.hasActiveStreams;
        
        pa_context_unref(ctx);
        pa_mainloop_free(ml);

        return data.audioAppsVector;
    }
};

void signal_handler(int sig) {
    (void)sig;
    keep_running = 0;
}

static size_t read_func(void* ptr, size_t size, size_t nmemb, void* datasource) {
    AudioData* ad = static_cast<AudioData*>(datasource);
    size_t bytes_to_read = size * nmemb;
    
    if(ad->position + bytes_to_read > ad->size)
        bytes_to_read = ad->size - ad->position;
    
    if (bytes_to_read > 0) {
        memcpy(ptr, ad->data + ad->position, bytes_to_read);
        ad->position += bytes_to_read;
    }
    return bytes_to_read;
}

static int seek_func(void* datasource, ogg_int64_t offset, int whence) {
    AudioData* ad = static_cast<AudioData*>(datasource);
    
    switch(whence) {
        case SEEK_SET: 
            ad->position = (offset <= 0) ? 0 : 
                         ((size_t)offset > ad->size) ? ad->size : (size_t)offset;
            break;
        case SEEK_CUR: 
            ad->position = (ad->position + offset <= 0) ? 0 :
                         ((ad->position + offset) > ad->size) ? ad->size : (ad->position + offset);
            break;
        case SEEK_END: 
            ad->position = (ad->size + offset <= 0) ? 0 :
                         ((ad->size + offset) > ad->size) ? ad->size : (ad->size + offset);
            break;
    }
    
    return 0;
}

static long tell_func(void* datasource) {
    AudioData* ad = static_cast<AudioData*>(datasource);
    return ad->position;
}

void play_track(int start_track) {
    current_track = start_track;
    
    while (keep_running) {
        if (current_track < 0 || static_cast<size_t>(current_track) >= audio_files.size()) {
            std::cerr << "Неверный индекс трека: " << current_track 
                      << "; всего треков: " << audio_files.size() << std::endl;
            current_track = 0;
            if (audio_files.empty() || static_cast<size_t>(current_track) >= audio_files.size()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
        }
        
        OggVorbis_File vf;
        ov_callbacks callbacks = {read_func, seek_func, nullptr, tell_func};
        if(ov_open_callbacks(&audio_files[current_track], &vf, nullptr, 0, callbacks) < 0) {
            std::cerr << "Ошибка открытия трека " << current_track << std::endl;
            if (audio_files.size() > 1) {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> dis(0, audio_files.size() - 1);
                current_track = dis(gen);
            } else {
                current_track = 0;
            }
            continue;
        }
        
        pa_sample_spec spec;
        spec.format = PA_SAMPLE_S16LE;
        spec.rate = 44100;
        spec.channels = 2;
        pa_simple* s = nullptr;
        char buffer[4096];
        int current_section;
        bool was_playing = true;
        
        while(keep_running) {
            if (!is_playing) {
                if (was_playing) {
                    std::cout << "Пауза: обнаружены другие звуковые потоки" << std::endl;
                    was_playing = false;
                    if (s) {
                        pa_simple_free(s);
                        s = nullptr;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            if (!was_playing) {
                std::cout << "Возобновление воспроизведения" << std::endl;
                was_playing = true;
            }
            if (!s) {
                s = pa_simple_new(
                    nullptr, 
                    "Space Ambient Daemon", 
                    PA_STREAM_PLAYBACK,
                    nullptr, 
                    "BackgroundDesktopSound", 
                    &spec, 
                    nullptr, 
                    nullptr, 
                    nullptr
                );
                if (!s) {
                    std::cerr << "Ошибка создания PulseAudio соединения" << std::endl;
                    ov_clear(&vf);
                    break;
                }
            }
            long ret = ov_read(&vf, buffer, sizeof(buffer), 0, 2, 1, &current_section);
            if(ret > 0) {
                if(pa_simple_write(s, buffer, ret, nullptr) < 0) {
                    std::cerr << "Ошибка воспроизведения аудио" << std::endl;
                    break;
                }
            } else if (ret == 0) {
                std::cout << "Трек завершен, переходим к случайному треку" << std::endl;
                if (audio_files.size() > 1) {
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<> dis(0, audio_files.size() - 1);
                    current_track = dis(gen);
                } else {
                    current_track = 0;
                }
                break;
            } else {
                std::cerr << "Ошибка чтения аудио данных" << std::endl;
                if (audio_files.size() > 1) {
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<> dis(0, audio_files.size() - 1);
                    current_track = dis(gen);
                } else {
                    current_track = 0;
                }
                break;
            }
        }
        
        if (s) {
            pa_simple_drain(s, nullptr);
            pa_simple_free(s);
        }
        ov_clear(&vf);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main(int argc, char* argv[]) {
    setenv("QT_QPA_PLATFORM", "wayland", 1);
    
    QGuiApplication app(argc, argv);
    
    init_audio_files();
    
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    AudioMonitor monitor;
    
    std::thread audio_thread([&]() {
        play_track(current_track);
    });
    
    int result = app.exec();
    
    keep_running = 0;
    if (audio_thread.joinable()) {
        audio_thread.join();
    }
    
    return result;
}

#include "main.moc"