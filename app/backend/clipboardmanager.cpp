#include "clipboardmanager.h"
#include "nvcomputer.h"
#include "nvhttp.h"
#include "settings/artemissettings.h"
#include <QGuiApplication>
#include <QMimeData>
#include <QDebug>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QHttpMultiPart>
#include <QCryptographicHash>

// Matches Android CLIPBOARD_IDENTIFIER constant
const QString ClipboardManager::CLIPBOARD_IDENTIFIER = "artemis_qt_clipboard_sync";

// Static instance for singleton pattern
ClipboardManager* ClipboardManager::s_instance = nullptr;

ClipboardManager::ClipboardManager(QObject *parent)
    : QObject(parent)
    , m_clipboard(QGuiApplication::clipboard())
    , m_computer(nullptr)
    , m_http(nullptr)
    , m_smartSyncEnabled(false)
    , m_bidirectionalSync(true)
    , m_showToast(true)
    , m_hideContent(false)
    , m_maxClipboardSize(DEFAULT_MAX_SIZE)
    , m_enabled(false)
    , m_connected(false)
    , m_textOnlyMode(true)
    , m_maxContentSizeMB(1)
    , m_showNotifications(true)
    , m_syncInProgress(false)
{
    // Connect to clipboard changes (matches Android behavior)
    connect(m_clipboard, &QClipboard::dataChanged, this, &ClipboardManager::onClipboardChanged);
    
    // Load settings from persistent storage
    loadSettings();
}

void ClipboardManager::loadSettings()
{
    auto settings = ArtemisSettings::instance();
    
    // Load clipboard sync settings
    m_enabled = settings->clipboardSyncEnabled();
    m_smartSyncEnabled = m_enabled; // Smart sync follows enabled state
    m_bidirectionalSync = settings->clipboardSyncBidirectional();
    m_maxClipboardSize = settings->clipboardSyncMaxSize();
    m_maxContentSizeMB = m_maxClipboardSize / (1024 * 1024); // Convert bytes to MB
    
    qDebug() << "ClipboardManager: Loaded settings - enabled:" << m_enabled 
             << "bidirectional:" << m_bidirectionalSync 
             << "maxSize:" << m_maxClipboardSize << "bytes";
}

ClipboardManager::~ClipboardManager()
{
    disconnect();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

ClipboardManager* ClipboardManager::instance()
{
    if (!s_instance) {
        s_instance = new ClipboardManager();
    }
    return s_instance;
}

ClipboardManager* ClipboardManager::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    return instance();
}

void ClipboardManager::setConnection(NvComputer *computer, NvHTTP *http)
{
    m_computer = computer;
    m_http = http;
    
    bool wasConnected = m_connected;
    m_connected = (computer != nullptr && http != nullptr);
    
    if (wasConnected != m_connected) {
        emit connectionChanged();
    }
    
    qDebug() << "ClipboardManager: Connected to" << (computer ? computer->name : "null");
    
    // Check and emit Apollo support status
    bool supported = isClipboardSyncSupported();
    emit apolloSupportChanged(supported);
    
    if (supported) {
        qDebug() << "ClipboardManager: Apollo server detected - clipboard sync available";
    } else {
        qDebug() << "ClipboardManager: Non-Apollo server - clipboard sync not available";
    }
}

void ClipboardManager::disconnect()
{
    m_computer = nullptr;
    m_http = nullptr;
    m_syncInProgress = false;
    
    bool wasConnected = m_connected;
    m_connected = false;
    
    if (wasConnected) {
        emit connectionChanged();
    }
    
    emit apolloSupportChanged(false);
    
    qDebug() << "ClipboardManager: Disconnected";
}

bool ClipboardManager::sendClipboard(bool force)
{
    if (!m_http || !m_computer) {
        qWarning() << "ClipboardManager: No connection available for clipboard sync";
        return false;
    }

    if (!isClipboardSyncSupported()) {
        qWarning() << "ClipboardManager: Clipboard sync not supported (not an Apollo server)";
        emit clipboardSyncFailed("Clipboard sync only works with Apollo/Sunshine servers");
        return false;
    }

    if (m_syncInProgress) {
        qDebug() << "ClipboardManager: Sync already in progress, skipping";
        return false;
    }

    QString clipboardText = getClipboardContent(force);
    if (clipboardText.isNull()) {
        qDebug() << "ClipboardManager: No clipboard content to send";
        return false;
    }

    return sendClipboardToServer(clipboardText);
}

bool ClipboardManager::getClipboard()
{
    if (!m_http || !m_computer) {
        qWarning() << "ClipboardManager: No connection available for clipboard sync";
        return false;
    }

    if (!isClipboardSyncSupported()) {
        qWarning() << "ClipboardManager: Clipboard sync not supported (not an Apollo server)";
        emit clipboardSyncFailed("Clipboard sync only works with Apollo/Sunshine servers");
        return false;
    }

    if (m_syncInProgress) {
        qDebug() << "ClipboardManager: Sync already in progress, skipping";
        return false;
    }

    QString clipboardContent = getClipboardFromServer();
    if (!clipboardContent.isNull()) {
        setClipboardContent(clipboardContent);
        return true;
    }

    return false;
}

void ClipboardManager::enableSmartSync(bool enabled)
{
    m_smartSyncEnabled = enabled;
    qDebug() << "ClipboardManager: Smart sync" << (enabled ? "enabled" : "disabled");
}

bool ClipboardManager::isSmartSyncEnabled() const
{
    return m_smartSyncEnabled;
}

bool ClipboardManager::isClipboardSyncSupported() const
{
    if (!m_computer || !m_http) {
        return false;
    }

    // Based on Artemis Android implementation, we don't need complex Apollo detection.
    // Just return true and let the HTTP calls succeed or fail naturally.
    // The clipboard sync endpoints work with any server that supports them.
    return true;
}

void ClipboardManager::onStreamStarted()
{
    if (m_smartSyncEnabled) {
        qDebug() << "ClipboardManager: Stream started, uploading clipboard";
        sendClipboard(false);
    }
}

void ClipboardManager::onStreamResumed()
{
    if (m_smartSyncEnabled) {
        qDebug() << "ClipboardManager: Stream resumed, uploading clipboard";
        sendClipboard(false);
    }
}

void ClipboardManager::onFocusLost()
{
    if (m_smartSyncEnabled && m_bidirectionalSync) {
        qDebug() << "ClipboardManager: Focus lost, downloading clipboard";
        getClipboard();
    }
}

void ClipboardManager::setMaxClipboardSize(int maxSize)
{
    m_maxClipboardSize = maxSize;
}

int ClipboardManager::getMaxClipboardSize() const
{
    return m_maxClipboardSize;
}

void ClipboardManager::setBidirectionalSync(bool enabled)
{
    if (m_bidirectionalSync != enabled) {
        m_bidirectionalSync = enabled;
        
        // Save to persistent settings
        auto settings = ArtemisSettings::instance();
        settings->setClipboardSyncBidirectional(enabled);
        settings->save();
        
        emit bidirectionalSyncChanged();
        qDebug() << "ClipboardManager: Bidirectional sync changed to" << enabled;
    }
}

bool ClipboardManager::isBidirectionalSyncEnabled() const
{
    return m_bidirectionalSync;
}

void ClipboardManager::setShowToast(bool enabled)
{
    m_showToast = enabled;
}

bool ClipboardManager::shouldShowToast() const
{
    return m_showToast;
}

void ClipboardManager::setHideContent(bool enabled)
{
    m_hideContent = enabled;
}

bool ClipboardManager::shouldHideContent() const
{
    return m_hideContent;
}

void ClipboardManager::onClipboardChanged()
{
    if (m_syncInProgress) {
        return; // Ignore changes during sync
    }

    QString content = getClipboardContent(false);
    if (!content.isNull() && !isOwnClipboardChange(content)) {
        emit clipboardContentChanged();
        
        // Auto-upload if smart sync is enabled
        if (m_smartSyncEnabled) {
            sendClipboard(false);
        }
    }
}

QString ClipboardManager::getClipboardContent(bool force)
{
    if (!m_clipboard->mimeData()) {
        return QString();
    }

    const QMimeData *mimeData = m_clipboard->mimeData();
    if (!mimeData->hasText()) {
        return QString();
    }

    QString text = mimeData->text();
    
    // Check size limit
    if (text.size() > m_maxClipboardSize) {
        qWarning() << "ClipboardManager: Clipboard content too large:" << text.size() << "bytes";
        return QString();
    }

    // Check if this is our own content (loop prevention)
    if (!force && isOwnClipboardChange(text)) {
        qDebug() << "ClipboardManager: Ignoring own clipboard change";
        return QString();
    }

    return text;
}

void ClipboardManager::setClipboardContent(const QString &content)
{
    if (content.isEmpty()) {
        return;
    }

    m_syncInProgress = true;
    
    // Mark as our own content to prevent loops
    markAsOwnContent(content);
    
    // Set clipboard content
    QMimeData *mimeData = new QMimeData();
    mimeData->setText(content);
    
    if (m_hideContent) {
        // Mark as sensitive (matches Android behavior)
        mimeData->setData("application/x-qt-windows-mime;value=\"Clipboard Viewer Format\"", QByteArray());
    }
    
    m_clipboard->setMimeData(mimeData);
    m_lastReceivedContent = content;
    
    m_syncInProgress = false;
    
    qDebug() << "ClipboardManager: Set clipboard content (" << content.size() << " chars)";
}

bool ClipboardManager::isOwnClipboardChange(const QString &content)
{
    QString hash = generateContentHash(content);
    return m_ownContentHashes.contains(hash) || content == m_lastReceivedContent;
}

void ClipboardManager::markAsOwnContent(const QString &content)
{
    QString hash = generateContentHash(content);
    m_ownContentHashes.append(hash);
    
    // Keep only last 10 hashes to prevent memory growth
    if (m_ownContentHashes.size() > 10) {
        m_ownContentHashes.removeFirst();
    }
}

QString ClipboardManager::generateContentHash(const QString &content)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(content.toUtf8());
    return hash.result().toHex();
}

bool ClipboardManager::sendClipboardToServer(const QString &content)
{
    if (!m_http || content.isEmpty()) {
        return false;
    }

    emit clipboardSyncStarted();
    m_syncInProgress = true;

    // On Linux the session's NvHTTP lives on the streaming thread, which runs no Qt
    // event loop, while uploads are triggered from the GUI thread. A cross-thread
    // QNetworkAccessManager never gets its sslErrors handled and the TLS handshake
    // fails, so use a client owned by the calling thread.
    NvHTTP http(m_computer);
    bool success = http.sendClipboardContent(content);
    
    m_syncInProgress = false;
    
    if (success) {
        markAsOwnContent(content);
        emit clipboardSyncCompleted();
        
        if (m_showToast) {
            emit showToast("Clipboard uploaded to server");
        }
        
        qDebug() << "ClipboardManager: Successfully sent clipboard to server";
        return true;
    } else {
        emit clipboardSyncFailed("Failed to send clipboard to server");
        qWarning() << "ClipboardManager: Failed to send clipboard to server";
        return false;
    }
}

QString ClipboardManager::getClipboardFromServer()
{
    if (!m_http) {
        return QString();
    }

    emit clipboardSyncStarted();
    m_syncInProgress = true;

    // Same thread affinity issue as in sendClipboardToServer()
    NvHTTP http(m_computer);
    QString content = http.getClipboardContent();
    
    m_syncInProgress = false;
    
    if (!content.isEmpty()) {
        emit clipboardSyncCompleted();
        
        if (m_showToast) {
            emit showToast("Clipboard downloaded from server");
        }
        
        qDebug() << "ClipboardManager: Successfully received clipboard from server";
        return content;
    } else {
        emit clipboardSyncFailed("Failed to get clipboard from server");
        qWarning() << "ClipboardManager: Failed to get clipboard from server";
        return QString();
    }
}

// Property setters
void ClipboardManager::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        
        // Enable/disable smart sync when clipboard sync is enabled/disabled
        enableSmartSync(enabled);
        
        // Save to persistent settings
        auto settings = ArtemisSettings::instance();
        settings->setClipboardSyncEnabled(enabled);
        settings->save();
        
        emit enabledChanged();
        qDebug() << "ClipboardManager: Enabled changed to" << enabled << "(smart sync:" << (enabled ? "enabled" : "disabled") << ")";
    }
}

void ClipboardManager::setTextOnlyMode(bool textOnly)
{
    if (m_textOnlyMode != textOnly) {
        m_textOnlyMode = textOnly;
        emit textOnlyModeChanged();
        qDebug() << "ClipboardManager: Text-only mode changed to" << textOnly;
    }
}

void ClipboardManager::setMaxContentSizeMB(int sizeMB)
{
    if (m_maxContentSizeMB != sizeMB) {
        m_maxContentSizeMB = sizeMB;
        m_maxClipboardSize = sizeMB * 1024 * 1024; // Convert to bytes
        
        // Save to persistent settings
        auto settings = ArtemisSettings::instance();
        settings->setClipboardSyncMaxSize(m_maxClipboardSize);
        settings->save();
        
        emit maxContentSizeMBChanged();
        qDebug() << "ClipboardManager: Max content size changed to" << sizeMB << "MB";
    }
}

void ClipboardManager::setShowNotifications(bool show)
{
    if (m_showNotifications != show) {
        m_showNotifications = show;
        m_showToast = show; // Sync with internal setting
        emit showNotificationsChanged();
        qDebug() << "ClipboardManager: Show notifications changed to" << show;
    }
}