#include "gui.h"
#include "ui_gui.h"

static crystal::Processhandle process{};
static gui* instance = nullptr;

void ConsoleHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (instance && instance->consolewindow)
    {
        QMetaObject::invokeMethod(instance->consolewindow, [msg]() {
            instance->consolewindow->appendmessage(msg);
        }, Qt::QueuedConnection);
    }
}

gui::gui(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::gui)
{
    ui->setupUi(this);
    setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);

    instance = this;
    qInstallMessageHandler(ConsoleHandler);

    // - discord rich presence.
    crystal::discord::StartRichPresence();
    rpctimer = new QTimer(this);
    connect(rpctimer, &QTimer::timeout, []()
    {
        Discord_RunCallbacks();
    });
    rpctimer->start(2000);

    // - update discord rich precense based on page.
    connect(ui->pages, &QStackedWidget::currentChanged, this, [this](int index)
    {
        if (index == 0)
        {
            crystal::discord::SetRichPresenceImage("crystal");
            crystal::discord::RemoveRichPresenceSmall();
            crystal::discord::SetRichPresenceDetails("idling...");
        }
        else if (index == 1)
        {
            crystal::discord::SetRichPresenceDetails("choosing a version...");
        }
        crystal::discord::UpdateRichPresence();
    });

    // - cancel login button.
    ui->logincancelbutton->hide();

    // - microsoft login button styling.
    ui->loginmicrosoftbutton->setStyleSheet("background-color: white; color: #616161;");

    // - pages.
    ui->pages->setCurrentIndex(0);

    // - play offline button.
    connect(ui->offlinebutton, &QPushButton::clicked, [this]()
    {
        ui->pages->setCurrentIndex(1);
    });

    // - use latest login checkbox.
    connect(ui->latestlogin, &QCheckBox::toggled, this, [this](bool checked)
    {
        microsoftlatestlogin = checked;
    });

    // - back button.
    connect(ui->backbutton, &QPushButton::clicked, [this]()
    {
        ui->pages->setCurrentIndex(0);
        ui->usernameinput->setText("");
        ui->usernameinput->setEnabled(true);
    });

    // - console window and console button.
    consolewindow = new console(this);
    consolewindow->setWindowFlags(Qt::Window);

    connect(consolewindow, &QObject::destroyed, [this]()
    {
        consolewindow = nullptr;
    });
    consolewindow->hide();

    connect(ui->consolebutton, &QPushButton::clicked, [this]()
    {
        if (consolewindow)
        {
            consolewindow->show();
            consolewindow->raise();
            consolewindow->activateWindow();
        }
    });

    // - loader box.
    ui->loaderbox->setStyleSheet("QComboBox { combobox-popup: 0; }");
    ui->loaderbox->setMaxVisibleItems(10);
    ui->loaderbox->clear();
    ui->loaderbox->addItem("vanilla");
    ui->loaderbox->addItem("fabric");
    loaderselected = ui->loaderbox->currentText();
    connect(ui->loaderbox, &QComboBox::currentTextChanged, this, &gui::on_loadercombo_changed);

    // - version box.
    ui->versionbox->setStyleSheet("QComboBox { combobox-popup: 0; }");
    ui->versionbox->setMaxVisibleItems(10);
    GetVersions();
    connect(ui->versionbox, &QComboBox::currentTextChanged, this, &gui::on_versioncombo_changed);

    #ifdef _WIN32
    osselected = "windows";
    #endif

    #ifdef __linux__
    osselected = "linux";
    #endif

    #ifdef __APPLE__
    osselected = "macos";
    #endif

    #if defined(__x86_64__) || defined(_M_X64)
    archselected = "x64";
    #endif

    #if defined(__i386__) || defined(_M_IX86)
    archselected = "x32";
    #endif

    #if defined(__aarch64__) || defined(_M_ARM64)
    archselected = "arm64";
    #endif

    #if defined(__arm__) || defined(_M_ARM)
    archselected = "x32";
    #endif

    // - username input.
    connect(ui->usernameinput, &QLineEdit::textChanged, this, &gui::on_usernameinput_changed);
}

gui::~gui()
{
    crystal::auth::StopMicrosoftLoginListener();
    if (rpctimer)
    {
        rpctimer->stop();
    }
    Discord_Shutdown();
    qInstallMessageHandler(0);
    instance = nullptr;
    delete ui;
}

void gui::on_loadercombo_changed(const QString &loader)
{
    loaderselected = loader;
    GetVersions();
}

void gui::on_versioncombo_changed(const QString &version)
{
    versionselected = version;
}

void gui::on_oscombo_changed(const QString &os)
{
    osselected = os;
}

void gui::on_archcombo_changed(const QString &arch)
{
    archselected = arch;
}

void gui::on_usernameinput_changed(const QString &input)
{
    username = input;
}

void gui::GetVersions()
{
    ui->versionbox->blockSignals(true);
    ui->versionbox->clear();

    std::optional<std::vector<std::string>> versions;

    if (loaderselected == "vanilla")
    {
        if (versionsvanilla)
        {
            versions = versionsvanilla;
        }
        else
        {
            auto manifest = crystal::vanilla::DownloadVersionManifest();
            versions = crystal::vanilla::GetVersionsFromManifest(*manifest);
            versionsvanilla = versions;
        }
    }
    else if (loaderselected == "fabric")
    {
        if (versionsfabric)
        {
            versions = versionsfabric;
        }
        else
        {
            auto meta = crystal::fabric::DownloadVersionMeta();
            versions = crystal::fabric::GetVersionsFromMeta(*meta);
            versionsfabric = versions;
        }
    }

    if (!versions || versions->empty())
    {
        qDebug() << "Failed to get versions, using fallback.";

        ui->versionbox->clear();
        if (loaderselected == "vanilla")
        {
            ui->versionbox->addItem("1.21.11");
            ui->versionbox->addItem("1.8.9");
        }
        else if (loaderselected == "fabric")
        {
            ui->versionbox->addItem("1.21.11");
        }
        versionselected = ui->versionbox->currentText();
    }
    else
    {
        for (const auto &v : *versions)
        {
            ui->versionbox->addItem(QString::fromStdString(v));
        }
    }

    versionselected = ui->versionbox->currentText();
    ui->versionbox->blockSignals(false);
}

bool gui::StartVersion(const QString &username, const QString &loaderselected, const QString &versionselected, const QString &archselected, const QString &osselected)
{
    if (loaderselected == "vanilla")
    {
        // - download manifest.
        auto manifestopt = crystal::vanilla::DownloadVersionManifest();
        if (!manifestopt)
        {
            qDebug() << "Failed to download manifest.";
            return false;
        }
        auto manifest = *manifestopt;
        
        // - download version json.
        auto versionjsonurl = crystal::vanilla::GetVersionJsonDownloadUrl(manifest, versionselected.toStdString());
        if (!versionjsonurl)
        {
            qDebug() << "Version not found in manifest.";
            return false;
        }
        auto versionurl = *versionjsonurl;

        qDebug() << "Downloading version json...";
        auto versionjsonopt = crystal::vanilla::DownloadVersionJson(versionurl, versionselected.toStdString());
        if (!versionjsonopt)
        {
            qDebug() << "Failed to download version json (are you offline?).";
            return false;
        }
        qDebug() << "Version json downloaded.";
        auto versionjson = *versionjsonopt;

        // - download client jar.
        auto jarurlopt = crystal::vanilla::GetClientJarDownloadUrl(versionjson);
        if (!jarurlopt)
        {
            qDebug() << "Failed to get client jar url.";
            return false;
        }
        auto jarurl = *jarurlopt;

        qDebug() << "Downloading client jar...";
        auto clientjar = crystal::vanilla::DownloadClientJar(jarurl, versionselected.toStdString());
        if (!clientjar)
        {
            qDebug() << "Failed to download client jar.";
            return false;
        }
        qDebug() << "Client jar downloaded.";

        // - download asset index.
        auto indexurlopt = crystal::vanilla::GetAssetIndexJsonDownloadUrl(versionjson);
        if (!indexurlopt)
        {
            qDebug() << "Failed to get asset index URL.";
            return false;
        }
        auto indexurl = *indexurlopt;

        qDebug() << "Downloading Asset index...";
        auto assetjsonopt = crystal::vanilla::DownloadAssetIndexJson(indexurl, versionselected.toStdString());
        if (!assetjsonopt)
        {
            qDebug() << "Failed to download asset index.";
            return false;
        }
        auto assetjson = *assetjsonopt;
        qDebug() << "Asset index downloaded.";

        // - download assets.
        auto assetsurlopt = crystal::vanilla::GetAssetsDownloadUrl(assetjson);
        if (!assetsurlopt)
        {
            qDebug() << "Failed to get assets download urls.";
            return false;
        }

        qDebug() << "Downloading assets... (this may take a while)";
        auto assets = crystal::vanilla::DownloadAssets(*assetsurlopt, versionselected.toStdString());
        if (!assets)
        {
            qDebug() << "Failed to download assets.";
            return false;
        }
        qDebug() << "Assets downloaded.";

        // - conversion.
        crystal::OS osenum;
        if (osselected == "windows")
            osenum = crystal::OS::Windows;
        else if (osselected == "linux")
            osenum = crystal::OS::Linux;
        else if (osselected == "macos")
            osenum = crystal::OS::Macos;
        else
        {
            qDebug() << "Invalid OS.";
            return false;
        }
        crystal::Arch archenum;
        if (archselected == "x64")
            archenum = crystal::Arch::x64;
        else if (archselected == "x32")
            archenum = crystal::Arch::x32;
        else if (archselected == "arm64")
            archenum = crystal::Arch::arm64;
        else
        {
            qDebug() << "Invalid architecture.";
            return false;
        }

        // - download java.
        auto javaversionopt = crystal::GetJavaVersion(versionjson);
        if (!javaversionopt)
        {
            qDebug() << "Failed to get java version.";
            return false;
        }
        int javaversion = *javaversionopt;

        auto javaurlopt = crystal::GetJavaDownloadUrl(javaversion, osenum, archenum);
        if (!javaurlopt)
        {
            qDebug() << "Failed to get java download url.";
            return false;
        }
        auto javaurl = *javaurlopt;

         qDebug() << "Downloading java...";
        auto javaopt = crystal::DownloadJava(javaurl, versionselected.toStdString());
        if (!javaopt)
        {
            qDebug() << "Failed to download java.";
            return false;
        }
        qDebug() << "Java downloaded.";
        auto java = *javaopt;

        // - download libraries.
        auto librariesurlopt = crystal::vanilla::GetLibrariesDownloadUrl(versionjson, osenum);
        if (!librariesurlopt)
        {
            qDebug() << "Failed to get libraries.";
            return false;
        }

        qDebug() << "Downloading libraries...";
        auto librariesdownloaded = crystal::vanilla::DownloadLibraries(*librariesurlopt, versionselected.toStdString());
        if (!librariesdownloaded)
        {
            qDebug() << "Failed to download libraries.";
            return false;
        }
        qDebug() << "Libraries downloaded.";

        // - extract natives.
        qDebug() << "Extracting natives...";
        auto nativesurlopt = crystal::vanilla::GetLibrariesNatives(versionselected.toStdString(), versionjson, osenum, archenum);
        if (!nativesurlopt)
        {
            qDebug() << "Failed to get natives urls.";
            return false;
        }

        qDebug() <<"Downloading natives...";
        auto nativesjarsopt = crystal::vanilla::DownloadLibrariesNatives(*nativesurlopt, versionselected.toStdString());
        if (!nativesjarsopt)
        {
            qDebug() << "Failed to download native jars.";
            return false;
        }

        auto nativesextracted = crystal::vanilla::ExtractLibrariesNatives(*nativesjarsopt, versionselected.toStdString(), osenum);
        if (!nativesextracted)
        {
            qDebug() << "Failed to extract natives.";
            return false;
        }
        qDebug() << "Natives extracted.";

        // - build classpath.
        qDebug() << "Building classpath...";
        fs::path datapath = ".crystal";
        auto classpathopt = crystal::vanilla::GetClassPath(versionjson, *librariesdownloaded, (datapath / "versions" / versionselected.toStdString() / "client.jar").string(), osenum);
        if (!classpathopt)
        {
            qDebug() << "Failed to build classpath.";
            return false;
        }
        auto classpath = *classpathopt;
        qDebug() << "Classpath built.";

        // - build launch command depending on whether the user is offline or logged in.
        std::string launchcmd;
        if (microsoft)
        {
            qDebug() << "Building launch command...";
            auto launchcmdopt = crystal::vanilla::GetLaunchCommand(microsoftusername, classpath, versionjson, versionselected.toStdString(), osenum, microsoftuuid, microsoftaccesstoken, "msa");
            if (!launchcmdopt)
            {
                qDebug() << "Failed to build launch command.";
                return false;
            }
            launchcmd = *launchcmdopt;
            qDebug() << "Launch command built.";
        }
        else
        {
            qDebug() << "Building launch command...";
            auto launchcmdopt = crystal::vanilla::GetLaunchCommand(username.toStdString(), classpath, versionjson, versionselected.toStdString(), osenum);
            if (!launchcmdopt)
            {
                qDebug() << "Failed to build launch command.";
                return false;
            }
            launchcmd = *launchcmdopt;
            qDebug() << "Launch command built.";
        }

        // - launch minecraft.
        std::string javapath;
        switch (osenum)
        {
        case crystal::OS::Windows:
            javapath = "runtime/" + versionselected.toStdString() + "/java/bin/java.exe";
            break;

        case crystal::OS::Linux:
        case crystal::OS::Macos:
            javapath = "runtime/" + versionselected.toStdString() + "/java/bin/java";
            break;
        }
        bool launched = crystal::StartProcess(javapath, launchcmd, osenum, &process, true);
        if (!launched)
        {
            QMetaObject::invokeMethod(this, [this](){QMessageBox::critical(this, "error", "Failed to launch minecraft.");}, Qt::QueuedConnection);
            return false;
        }
        else
        {
            crystal::discord::SetRichPresenceImage("vanilla");
            crystal::discord::SetRichPresenceDetails("playing minecraft " + versionselected.toStdString());
            crystal::discord::UpdateRichPresence();
            QMetaObject::invokeMethod(this, [this](){QMessageBox::information(this, "info", "Minecraft launched.");}, Qt::QueuedConnection);
            return true;
        }
    }
    else if (loaderselected == "fabric")
    {
        crystal::discord::SetRichPresenceImage("fabric");
        crystal::discord::SetRichPresenceDetails("launching fabric " + versionselected.toStdString());
        crystal::discord::UpdateRichPresence();
        // - download fabric meta.
        auto meta = crystal::fabric::DownloadVersionMeta();

        // - download meta json that contains the loader version.
        auto loadermetaurlopt = crystal::fabric::GetLoaderMetaUrl(versionselected.toStdString());
        if (!loadermetaurlopt)
        {
            qDebug() << "Failed to get loader meta url (are you offline?).";
            return false;
        }
        auto loadermetaurl = *loadermetaurlopt;

        auto loadermetaopt = crystal::fabric::DownloadLoaderMeta(loadermetaurl);
        if (!loadermetaopt)
        {
            qDebug() << "Failed to download loader meta.";
            return false;
        }
        auto loadermeta = *loadermetaopt;
        qDebug() << "Loader meta downloaded.";

        auto loaderopt = crystal::fabric::GetLoaderVersion(loadermeta);
        if (!loaderopt)
        {
            qDebug() << "Failed to get loader version.";
            return false;
        }
        auto loader = *loaderopt;
        qDebug() << "Meta downloaded.";

        QString versionid = versionselected + "-fabric-loader-" + QString::fromStdString(loader);

        // - download fabric version json.
        auto versionjsonurl = crystal::fabric::GetLoaderJsonDownloadUrl(loader, versionselected.toStdString());
        if (!versionjsonurl)
        {
            qDebug() << "Version not found in fabric manifest.";
            return false;
        }
        auto versionurl = *versionjsonurl;

        qDebug() << "Downloading version json...";
        auto loaderjsonopt = crystal::fabric::DownloadLoaderJson(versionurl, loader, versionselected.toStdString());
        if (!loaderjsonopt)
        {
            qDebug() << "Failed to download version json.";
            return false;
        }
        auto loaderjson = *loaderjsonopt;
        qDebug() << "Version json downloaded.";

        // - merge vanilla json with fabric json.
        auto mergedjsonopt = crystal::fabric::GetLoaderJson(loaderjson, loader, versionselected.toStdString());
        if (!mergedjsonopt)
        {
            qDebug() << "Failed to create merged version json.";
            return false;
        }
        auto mergedjson = *mergedjsonopt;

        // - download client jar.
        auto jarurlopt = crystal::vanilla::GetClientJarDownloadUrl(mergedjson);
        if (!jarurlopt)
        {
            qDebug() << "Failed to get client jar url.";
            return false;
        }
        auto jarurl = *jarurlopt;

        qDebug() << "Downloading client jar...";
        auto clientjar = crystal::vanilla::DownloadClientJar(jarurl, versionid.toStdString());
        if (!clientjar)
        {
            qDebug() << "Failed to download client jar.";
            return false;
        }
        qDebug() << "Client jar downloaded.";

        // - download asset index.
        auto indexurlopt = crystal::vanilla::GetAssetIndexJsonDownloadUrl(mergedjson);
        if (!indexurlopt)
        {
            qDebug() << "Failed to get asset index URL.";
            return false;
        }
        auto indexurl = *indexurlopt;

        qDebug() << "Downloading Asset index...";
        auto assetjsonopt = crystal::vanilla::DownloadAssetIndexJson(indexurl, versionid.toStdString());
        if (!assetjsonopt)
        {
            qDebug() << "Failed to download asset index.";
            return false;
        }
        auto assetjson = *assetjsonopt;
        qDebug() << "Asset index downloaded.";

        // - download assets.
        auto assetsurlopt = crystal::vanilla::GetAssetsDownloadUrl(assetjson);
        if (!assetsurlopt)
        {
            qDebug() << "Failed to get assets download urls.";
            return false;
        }

        qDebug() << "Downloading assets... (this may take a while)";
        auto assets = crystal::vanilla::DownloadAssets(*assetsurlopt, versionid.toStdString());
        if (!assets)
        {
            qDebug() << "Failed to download assets.";
            return false;
        }
        qDebug() << "Assets downloaded.";

        // - conversion.
        crystal::OS osenum;
        if (osselected == "windows")
            osenum = crystal::OS::Windows;
        else if (osselected == "linux")
            osenum = crystal::OS::Linux;
        else if (osselected == "macos")
            osenum = crystal::OS::Macos;
        else
        {
            qDebug() << "Invalid OS.";
            return false;
        }
        crystal::Arch archenum;
        if (archselected == "x64")
            archenum = crystal::Arch::x64;
        else if (archselected == "x32")
            archenum = crystal::Arch::x32;
        else if (archselected == "arm64")
            archenum = crystal::Arch::arm64;
        else
        {
            qDebug() << "Invalid architecture.";
            return false;
        }

        // - download java.
        auto javaversionopt = crystal::GetJavaVersion(mergedjson);
        if (!javaversionopt)
        {
            qDebug() << "Failed to get java version.";
            return false;
        }
        int javaversion = *javaversionopt;

        auto javaurlopt = crystal::GetJavaDownloadUrl(javaversion, osenum, archenum);
        if (!javaurlopt)
        {
            qDebug() << "Failed to get java download url.";
            return false;
        }
        auto javaurl = *javaurlopt;

        qDebug() << "Downloading java...";
        auto javaopt = crystal::DownloadJava(javaurl, versionid.toStdString());
        if (!javaopt)
        {
            qDebug() << "Failed to download java.";
            return false;
        }
        qDebug() << "Java downloaded.";
        auto java = *javaopt;

        // - download libraries.
        auto librariesurlopt = crystal::fabric::GetLoaderLibrariesDownloadUrl(mergedjson, osenum);
        if (!librariesurlopt)
        {
            qDebug() << "Failed to get libraries.";
            return false;
        }

        qDebug() << "Downloading libraries...";
        auto librariesdownloaded = crystal::vanilla::DownloadLibraries(*librariesurlopt, versionid.toStdString());
        if (!librariesdownloaded)
        {
            qDebug() << "Failed to download libraries.";
            return false;
        }
        qDebug() << "Libraries downloaded.";

        // - extract natives.
        qDebug() << "Extracting natives...";
        auto nativesurlopt = crystal::vanilla::GetLibrariesNatives(versionid.toStdString(), mergedjson, osenum, archenum);
        if (!nativesurlopt)
        {
            qDebug() << "Failed to get natives urls.";
            return false;
        }

        qDebug() <<"Downloading natives...";
        auto nativesjarsopt = crystal::vanilla::DownloadLibrariesNatives(*nativesurlopt, versionid.toStdString());
        if (!nativesjarsopt)
        {
            qDebug() << "Failed to download native jars.";
            return false;
        }

        auto nativesextracted = crystal::vanilla::ExtractLibrariesNatives(*nativesjarsopt, versionid.toStdString(), osenum);
        if (!nativesextracted)
        {
            qDebug() << "Failed to extract natives.";
            return false;
        }
        qDebug() << "Natives extracted.";

        // - build classpath.
        qDebug() << "Building classpath...";
        fs::path datapath = ".crystal";
        auto classpathopt = crystal::vanilla::GetClassPath(mergedjson, *librariesdownloaded, (datapath / "versions" / versionid.toStdString() / "client.jar").string(), osenum);
        if (!classpathopt)
        {
            qDebug() << "Failed to build classpath.";
            return false;
        }
        auto classpath = *classpathopt;
        qDebug() << "Classpath built.";

        // - build launch command depending on whether the user is offline or logged in.
        std::string launchcmd;
        if (microsoft)
        {
            qDebug() << "Building launch command...";
            auto launchcmdopt = crystal::vanilla::GetLaunchCommand(microsoftusername, classpath, mergedjson, versionid.toStdString(), osenum, microsoftuuid, microsoftaccesstoken, "msa");
            if (!launchcmdopt)
            {
                qDebug() << "Failed to build launch command.";
                return false;
            }
            launchcmd = *launchcmdopt;
            qDebug() << "Launch command built.";
        }
        else
        {
            qDebug() << "Building launch command...";
            auto launchcmdopt = crystal::vanilla::GetLaunchCommand(username.toStdString(), classpath, mergedjson, versionid.toStdString(), osenum);
            if (!launchcmdopt)
            {
                qDebug() << "Failed to build launch command.";
                return false;
            }
            launchcmd = *launchcmdopt;
            qDebug() << "Launch command built.";
        }

        // - launch minecraft.
        std::string javapath;
        switch (osenum)
        {
        case crystal::OS::Windows:
            javapath = "runtime/" + versionid.toStdString() + "/java/bin/java.exe";
            break;

        case crystal::OS::Linux:
        case crystal::OS::Macos:
            javapath = "runtime/" + versionid.toStdString() + "/java/bin/java";
            break;
        }
        bool launched = crystal::StartProcess(javapath, launchcmd, osenum, &process, true);
        if (!launched)
        {
            QMetaObject::invokeMethod(this, [this](){QMessageBox::critical(this, "error", "Failed to launch minecraft.");}, Qt::QueuedConnection);
            return false;
        }
        else
        {
            crystal::discord::SetRichPresenceDetails("playing minecraft fabric " + versionselected.toStdString());
            crystal::discord::UpdateRichPresence();
            QMetaObject::invokeMethod(this, [this](){QMessageBox::information(this, "info", "Minecraft launched.");}, Qt::QueuedConnection);
            return true;
        }
    }
    return false;
}

void gui::on_startbutton_clicked()
{
    if (processrunning.exchange(true))
        return;

    if (ui->usernameinput->text().trimmed().isEmpty())
    {
        QMetaObject::invokeMethod(this, [this](){QMessageBox::critical(this, "error", "Please enter a username.");}, Qt::QueuedConnection);
        processrunning = false;
        return;
    }

    if (!microsoft)
    {
        crystal::discord::SetRichPresenceSmall(username.toStdString());
        crystal::discord::UpdateRichPresence();
    }
    else if (microsoft)
    {
        crystal::discord::SetRichPresenceSmall(microsoftusername, microsoftuuid);
        crystal::discord::UpdateRichPresence();
    }

    ui->startbutton->setEnabled(false);

    QFuture<void> future = QtConcurrent::run([this]()
    {
        if (!StartVersion(username, loaderselected, versionselected, archselected, osselected))
        {
            processrunning = false;
            QMetaObject::invokeMethod(ui->startbutton, "setEnabled", Qt::QueuedConnection, Q_ARG(bool, true));
            return;
        }
        while (crystal::DetectProcess(&process))
        {
            QThread::sleep(1);
        }
        crystal::discord::SetRichPresenceImage("crystal");
        crystal::discord::RemoveRichPresenceSmall();
        crystal::discord::SetRichPresenceDetails("choosing a version...");
        crystal::discord::UpdateRichPresence();
        processrunning = false;
        QMetaObject::invokeMethod(ui->startbutton, "setEnabled", Qt::QueuedConnection, Q_ARG(bool, true));
    });
}

void gui::on_stopbutton_clicked()
{
    if (!crystal::StopProcess(&process))
    {
        QMessageBox::critical(this, "error", "Failed to stop minecraft.");
    }
    else
    {
        crystal::discord::SetRichPresenceDetails("choosing a version...");
        crystal::discord::UpdateRichPresence();
        QMessageBox::information(this, "info", "Minecraft stopped.");
        processrunning = false;
        ui->startbutton->setEnabled(true);
    }
}

bool gui::StartMicrosoftLogin()
{
    std::string accesstoken;
    if (!microsoftlogin)
    {
        // - get microsoft login url.
        auto codeurlopt = crystal::auth::GetMicrosoftLoginUrl();
        if (!codeurlopt)
        {
            qDebug() << "Failed to get auth url.";
            return false;
        }
        auto codeurl = *codeurlopt;

        // - start microsoft login listener for auth code.
        qDebug() << "180 seconds until login gets cancelled. Please press the cancel button if you want to retry the login process.";
        auto codeopt = crystal::auth::StartMicrosoftLoginListener(codeurl);
        if (!codeopt)
        {
            qDebug() << "Failed to login with Microsoft.";
            return false;
        }
        auto code = *codeopt;
        if (code == "timeout")
        {
            qDebug() << "Login canceled (timeout reached).";
            return false;
        }

        // - get access token json.
        auto accessjsonopt = crystal::auth::GetAccessTokenJson(code);
        if (!accessjsonopt)
        {
            qDebug() << "Failed to get access token json.";
            return false;
        }
        auto accessjson = *accessjsonopt;

        // - get access token from json.
        auto accesstokenopt = crystal::auth::GetAccessTokenFromJson(accessjson);
        if (!accesstokenopt)
        {
            qDebug() << "Failed to get access token.";
            return false;
        }
        accesstoken = *accesstokenopt;

        // - get refresh token from json.
        auto refreshtokenopt = crystal::auth::GetRefreshTokenFromJson(accessjson);
        if (!refreshtokenopt)
        {
            qDebug() << "Failed to get access token.";
            return false;
        }
        auto refreshtoken = *refreshtokenopt;
    }
    if (microsoftlogin)
    {
        // - get access token from refresh token.
        auto accesstokenopt = crystal::auth::GetAccessTokenFromRefreshToken();
        if (!accesstokenopt)
        {
            qDebug() << "Failed to get access token.";
            return false;
        }
        accesstoken = *accesstokenopt;
    }

    // - get xbox token json.
    auto xboxjsonopt = crystal::auth::GetXboxTokenJson(accesstoken);
    if (!xboxjsonopt)
    {
        qDebug() << "Failed to get xbox token json.";
        return false;
    }
    auto xboxjson = *xboxjsonopt;

    // - get xbox token from json.
    auto xboxtokenopt = crystal::auth::GetXboxTokenFromJson(xboxjson);
    if (!xboxtokenopt)
    {
        qDebug() << "Failed to get xbox token from json.";
        return false;
    }
    auto xboxtoken = *xboxtokenopt;

    // - get xbox hash from json.
    auto xboxhashopt = crystal::auth::GetXboxHashFromJson(xboxjson);
    if (!xboxhashopt)
    {
        qDebug() << "Failed to get xbox hash from json.";
        return false;
    }
    auto xboxhash = *xboxhashopt;

    // - get xsts token json.
    auto xstsjsonopt = crystal::auth::GetXstsTokenJson(xboxtoken);
    if (!xstsjsonopt)
    {
        qDebug() << "Failed to get xsts token json.";
        return false;
    }
    auto xstsjson = *xstsjsonopt;

    // - get xsts token from json.
    auto xststokenopt = crystal::auth::GetXstsTokenFromJson(xstsjson);
    if (!xststokenopt)
    {
        qDebug() << "Failed to get xsts token from json.";
        return false;
    }
    auto xststoken = *xststokenopt;

    // - get minecraft token json.
    auto mcjsonopt = crystal::auth::GetMinecraftTokenJson(xboxhash, xststoken);
    if (!mcjsonopt)
    {
        qDebug() << "Failed to get minecraft token json.";
        return false;
    }
    auto mcjson = *mcjsonopt;

    // - get minecraft token from json.
    auto mctokenopt = crystal::auth::GetMinecraftTokenFromJson(mcjson);
    if (!mctokenopt)
    {
        qDebug() << "Failed to get minecraft token from json.";
        return false;
    }
    auto mctoken = *mctokenopt;

    // - verify minecraft ownership.
    auto ownerjsonopt = crystal::auth::GetMinecraftOwnershipJson(mctoken);
    if (!ownerjsonopt)
    {
        qDebug() << "Failed to get minecraft entitlements json.";
        return false;
    }
    auto ownerjson = *ownerjsonopt;

    auto owns = crystal::auth::GetMinecraftOwnershipFromJson(ownerjson);
    if (!owns.value())
    {
        QMetaObject::invokeMethod(this, [this](){QMessageBox::critical(this, "error", "This account doesn't own minecraft.");}, Qt::QueuedConnection);
        loginfailedmessage = false;
        return false;
    }

    // - get minecraft profile json.
    auto profileopt = crystal::auth::GetMinecraftProfileJson(mctoken);
    if (!profileopt)
    {
        qDebug() << "Failed to get minecraft profile json.";
        return false;
    }
    auto profilejson = *profileopt;

    // - get minecraft username from profile json.
    auto usernameopt = crystal::auth::GetUsernameFromProfileJson(profilejson);
    if (!usernameopt)
    {
        qDebug() << "Failed to get minecraft username from profile json.";
        return false;
    }
    auto username = *usernameopt;

    // - get minecraft uuid from profile json.
    auto uuidopt = crystal::auth::GetUuidFromProfileJson(profilejson);
    if (!uuidopt)
    {
        qDebug() << "Failed to get minecraft uuid from profile json.";
        return false;
    }
    auto uuid = *uuidopt;

    qDebug() << QString::fromStdString(username);
    qDebug() << QString::fromStdString(uuid);

    microsoftusername = username;
    microsoftuuid = uuid;
    microsoftaccesstoken = mctoken;

    ui->usernameinput->setText(QString::fromStdString(username));
    ui->usernameinput->setEnabled(false);
    return true;
}

void gui::on_loginmicrosoftbutton_clicked()
{
    if (loginrunning.exchange(true))
        return;

    if (microsoftlatestlogin)
    {
        microsoftlogin = fs::exists(".crystal/refresh_token");
    }
    else
    {
        microsoftlogin = false;
    }

    if (!microsoftlogin)
    {
        ui->logincancelbutton->show();
        microsoft = true;
    }
    ui->loginmicrosoftbutton->setEnabled(false);
    ui->offlinebutton->setEnabled(false);

    loginfailedmessage = true;

    QFuture<void> future = QtConcurrent::run([this]()
    {
        bool success = StartMicrosoftLogin();

        QMetaObject::invokeMethod(this, [this, success]()
        {
            loginrunning = false;

            if (!microsoftlogin)
            {
                ui->logincancelbutton->hide();
            }
            ui->loginmicrosoftbutton->setEnabled(true);
            ui->offlinebutton->setEnabled(true);

            if (!success && loginfailedmessage)
            {
                QMessageBox::critical(this, "error", "Login failed.");
            }
            else if (success)
            {
                qDebug() << "Logged in.";
                ui->pages->setCurrentIndex(1);
                QMessageBox::information(this, "info", "Logged in as: " + QString::fromStdString(microsoftusername));
            }
        }, Qt::QueuedConnection);
    });
}

void gui::on_logincancelbutton_clicked()
{
    bool running = crystal::auth::StopMicrosoftLoginListener();
    if (running)
        qDebug() << "Login canceled.";

    ui->logincancelbutton->hide();
    ui->loginmicrosoftbutton->setEnabled(true);
    ui->offlinebutton->setEnabled(true);
    loginrunning = false;
    microsoft = false;
}
