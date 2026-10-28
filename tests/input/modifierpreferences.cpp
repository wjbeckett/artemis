#include "settings/streamingpreferences.h"
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

// No desktop is needed to exercise real preference loading, saving and signals.
namespace WMUtils { bool isRunningWayland() { return false; } }

class ModifierPreferencesTest : public QObject
{
    Q_OBJECT
    QTemporaryDir directory;
private slots:
    void initTestCase()
    {
        QVERIFY(directory.isValid());
        QCoreApplication::setOrganizationName("ArtemisModifierTests");
        QCoreApplication::setApplicationName("Preferences");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
    }
    void init()
    {
        QSettings().clear();
        StreamingPreferences::get()->reload();
    }
    void defaultsAndPresets()
    {
        auto prefs = StreamingPreferences::get();
        QVERIFY(prefs->modifierMapping.isIdentity());
        QSignalSpy signal(prefs, &StreamingPreferences::modifierMappingChanged);
        prefs->setModifierPreset(2);
        QCOMPARE(prefs->controlModifier(), 0);
        QCOMPARE(prefs->optionModifier(), 2);
        QCOMPARE(prefs->commandModifier(), 1);
        QCOMPARE(signal.count(), 1);
        prefs->setModifierPreset(2);
        QCOMPARE(signal.count(), 1);
        prefs->setModifierPreset(1);
        QCOMPARE(prefs->controlModifier(), 2);
        QCOMPARE(prefs->optionModifier(), 1);
        QCOMPARE(prefs->commandModifier(), 0);
        prefs->setModifierPreset(0);
        QVERIFY(prefs->modifierMapping.isIdentity());
    }
    void persistenceAndPropertyWrites()
    {
        auto prefs = StreamingPreferences::get();
        QSignalSpy signal(prefs, &StreamingPreferences::modifierMappingChanged);
        QVERIFY(prefs->setProperty("optionModifier", 2));
        QCOMPARE(prefs->optionModifier(), 2);
        QCOMPARE(prefs->commandModifier(), 1);
        QCOMPARE(signal.count(), 1);
        prefs->save();
        prefs->setModifierPreset(0);
        prefs->reload();
        QCOMPARE(prefs->controlModifier(), 0);
        QCOMPARE(prefs->optionModifier(), 2);
        QCOMPARE(prefs->commandModifier(), 1);
        QVERIFY(!QSettings().contains("swapcmdctrl"));
    }
    void legacyMigration()
    {
        QSettings settings;
        settings.setValue("swapcmdctrl", true);
        auto prefs = StreamingPreferences::get();
        prefs->reload();
        QCOMPARE(prefs->controlModifier(), 2);
        QCOMPARE(prefs->commandModifier(), 0);
        prefs->save();
        QVERIFY(!settings.contains("swapcmdctrl"));
        prefs->reload();
        QCOMPARE(prefs->controlModifier(), 2);
        // New mappings take precedence over a stale legacy preference.
        prefs->setModifierPreset(2);
        prefs->save();
        settings.setValue("swapcmdctrl", true);
        prefs->reload();
        QCOMPARE(prefs->controlModifier(), 0);
        QCOMPARE(prefs->optionModifier(), 2);
    }
    void invalidSettingsAndTargets()
    {
        auto prefs = StreamingPreferences::get();
        QSettings settings;
        for (const QVariant& value : {QVariant(-1), QVariant(99), QVariant("invalid"), QVariant(1)}) {
            settings.setValue("controlmodifier", value);
            settings.setValue("optionmodifier", 1);
            settings.setValue("commandmodifier", 2);
            prefs->reload();
            QVERIFY(prefs->modifierMapping.isIdentity());
        }
        QSignalSpy signal(prefs, &StreamingPreferences::modifierMappingChanged);
        prefs->setControlModifier(-1);
        prefs->setOptionModifier(3);
        prefs->setCommandModifier(99);
        prefs->setModifierPreset(99);
        QCOMPARE(signal.count(), 0);
        QVERIFY(prefs->modifierMapping.isIdentity());
    }
};
QTEST_GUILESS_MAIN(ModifierPreferencesTest)
#include "modifierpreferences.moc"
