#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "filepicker.h"
#include "localfiles.h"
#include "theme.h"
#include "translator.h"

// Stands in for the portal, so tests can "pick" a file without a desktop.
class FakePicker : public FilePicker {
    Q_OBJECT
public:
    using FilePicker::FilePicker;
    QString startFolder;
    void openMarkdown(const QString &folder) override { startFolder = folder; }
};

class MdviewTests : public QObject {
    Q_OBJECT

private slots:
    void opensAFile()
    {
        QTemporaryDir dir;
        write(dir.filePath("README.md"), "# Hello\n");
        Backend backend;
        QVERIFY(backend.open(dir.filePath("README.md")));
        QCOMPARE(backend.text(), QString("# Hello\n"));
        QCOMPARE(backend.fileName(), QString("README.md"));
        QVERIFY(backend.baseUrl().startsWith("file://"));
        QVERIFY(backend.baseUrl().endsWith("/"));
        QVERIFY(!backend.reloaded());
        QVERIFY(backend.error().isEmpty());
    }

    void opensAFileUrl()
    {
        QTemporaryDir dir;
        write(dir.filePath("a b.md"), "x");
        Backend backend;
        QVERIFY(backend.open(QUrl::fromLocalFile(dir.filePath("a b.md")).toString()));
        QCOMPARE(backend.fileName(), QString("a b.md"));
    }

    void saysWhatIsWrong()
    {
        QTemporaryDir dir;
        Backend backend;
        QVERIFY(!backend.open(dir.filePath("missing.md")));
        QVERIFY(backend.error().contains("does not exist"));
        QVERIFY(!backend.open(dir.path()));
        QVERIFY(backend.error().contains("folder"));
        backend.clearError();
        QVERIFY(backend.error().isEmpty());
    }

    void reloadsWhenTheFileChanges()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("notes.md");
        write(path, "one");
        Backend backend;
        QVERIFY(backend.open(path));
        const int revision = backend.revision();

        write(path, "two");
        QTRY_COMPARE(backend.text(), QString("two"));
        QVERIFY(backend.revision() > revision);
        QVERIFY(backend.reloaded());
    }

    void reloadsAfterAnAtomicSave()
    {
        // Editors write a new file and rename it over the old one.
        QTemporaryDir dir;
        const QString path = dir.filePath("notes.md");
        write(path, "one");
        Backend backend;
        QVERIFY(backend.open(path));

        write(dir.filePath(".notes.md.swp"), "two");
        QVERIFY(QFile::remove(path));
        QVERIFY(QFile::rename(dir.filePath(".notes.md.swp"), path));
        QTRY_COMPARE(backend.text(), QString("two"));

        write(path, "three");
        QTRY_COMPARE(backend.text(), QString("three"));
    }

    void keepsTheLastVersionWhenTheFileGoes()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("notes.md");
        write(path, "kept");
        Backend backend;
        QVERIFY(backend.open(path));
        QVERIFY(QFile::remove(path));
        QTRY_VERIFY(backend.missing());
        QCOMPARE(backend.text(), QString("kept"));

        write(path, "back");
        QTRY_COMPARE(backend.text(), QString("back"));
        QVERIFY(!backend.missing());
    }

    void followsLinksAndGoesBack()
    {
        QTemporaryDir dir;
        write(dir.filePath("a.md"), "a");
        write(dir.filePath("b.md"), "b");
        Backend backend;
        QVERIFY(backend.open(dir.filePath("a.md")));
        QVERIFY(!backend.canGoBack());
        QVERIFY(backend.follow(QUrl::fromLocalFile(dir.filePath("b.md")).toString()));
        QCOMPARE(backend.text(), QString("b"));
        QVERIFY(backend.canGoBack());
        QVERIFY(!backend.follow(dir.filePath("nope.md")));
        QCOMPARE(backend.text(), QString("b"));
        QVERIFY(backend.goBack());
        QCOMPARE(backend.text(), QString("a"));
        QVERIFY(!backend.canGoBack());
    }

    void recognizesMarkdown()
    {
        QVERIFY(Backend::isMarkdownUrl("file:///x/README.md"));
        QVERIFY(Backend::isMarkdownUrl("/x/notes.MARKDOWN"));
        QVERIFY(!Backend::isMarkdownUrl("file:///x/image.png"));
    }

    void picksThroughThePicker()
    {
        QTemporaryDir dir;
        write(dir.filePath("a.md"), "a");
        write(dir.filePath("b.md"), "b");
        FakePicker picker;
        Backend backend(&picker);
        QVERIFY(backend.open(dir.filePath("a.md")));
        backend.chooseFile();
        QCOMPARE(picker.startFolder, QFileInfo(dir.filePath("a.md")).absolutePath());
        emit picker.openSelected(QUrl::fromLocalFile(dir.filePath("b.md")));
        QCOMPARE(backend.text(), QString("b"));
        emit picker.failed("No portal");
        QCOMPARE(backend.error(), QString("No portal"));
    }

    void reportsAMissingEditor()
    {
        QTemporaryDir dir;
        write(dir.filePath("a.md"), "a");
        Backend backend(nullptr, nullptr, {"mdview-no-such-editor"});
        QVERIFY(backend.open(dir.filePath("a.md")));
        backend.openInEditor();
        QVERIFY(backend.error().contains("mdview-no-such-editor"));
    }

    void servesOnlyMedia()
    {
        QTemporaryDir dir;
        write(dir.filePath("pic.png"), "\x89PNG\r\n\x1a\n");
        write(dir.filePath("secret.txt"), "secret");
        QVERIFY(LocalFiles::mediaType(dir.filePath("pic.png")).startsWith("image/"));
        QVERIFY(LocalFiles::mediaType(dir.filePath("secret.txt")).isEmpty());
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
        QCOMPARE(theme.mode(), QString("dark"));
    }

    void themeReadsThePalette()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml",
              "mode = \"light\"\naccent = \"#112233\"\nbackground = \"#eff1f5\"\nforeground = \"#4c4f69\"\nname = \"not a color\"\n");

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
        QCOMPARE(theme.mode(), QString("light"));
        QCOMPARE(theme.background(), QString("#eff1f5"));
        QCOMPARE(theme.colors().value("foreground").toString(), QString("#4c4f69"));
        QVERIFY(!theme.colors().contains("name"));
    }

    void themeFollowsChanges()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\n");
        Theme theme(dir.path());
        write(dir.path() + "/theme/colors.toml", "accent = \"#445566\"\n");
        QTRY_COMPARE(theme.accent(), QString("#445566"));
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n");
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        write(response, "{\"int\":0}");

        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        write(response, "{\"int\":6}");
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Reload").isNull());
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        write(dir.path() + "/pt.json", R"({"Reload": "Recarregar", "Quit": "Sair"})");
        write(dir.path() + "/pt-BR.json", R"({"Reload": "Atualizar"})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("pt-BR"));
        QCOMPARE(translator.translate("Main", "Reload"), QString("Atualizar"));
        QCOMPARE(translator.translate("Main", "Quit"), QString("Sair"));
        QVERIFY(translator.translate("Main", "Find").isNull());
    }

    void shippedCatalogsAreValid()
    {
        // Every bundled translation parses and keeps its %1 placeholders.
        const QDir catalogs(QStringLiteral(SRCDIR "/../src/i18n"));
        const auto files = catalogs.entryList({"*.json"});
        QVERIFY(!files.isEmpty());
        for (const auto &name : files) {
            QFile file(catalogs.filePath(name));
            QVERIFY(file.open(QIODevice::ReadOnly));
            QJsonParseError error;
            const auto object = QJsonDocument::fromJson(file.readAll(), &error).object();
            QVERIFY2(error.error == QJsonParseError::NoError, qPrintable(name));
            for (auto it = object.begin(); it != object.end(); ++it) {
                for (const auto &placeholder : {"%1", "%2"}) {
                    if (it.key().contains(placeholder) && it.value().isString())
                        QVERIFY2(it.value().toString().contains(placeholder), qPrintable(name + ": " + it.key()));
                }
            }
        }
    }

private:
    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(contents);
    }
};

QTEST_MAIN(MdviewTests)
#include "mdview_tests.moc"
