// https://github.com/KubaO/stackoverflown/tree/master/questions/opencv-21246766
#include <QtWidgets>
#include <opencv2/opencv.hpp>

Q_DECLARE_METATYPE(cv::Mat)

class Capture : public QObject {
    Q_OBJECT
    QBasicTimer m_timer;
    QScopedPointer<cv::VideoCapture> m_videoCapture;
public:
    Capture(QObject * parent = {}) : QObject(parent) {}
    Q_SIGNAL void started();
    Q_SLOT void start(int cam = {}) {
        if (!m_videoCapture)
            m_videoCapture.reset(new cv::VideoCapture(cam));
        if (m_videoCapture->isOpened()) {
            m_timer.start(0, this);
            emit started();
        }
    }
    Q_SLOT void stop() { m_timer.stop(); }
    Q_SIGNAL void matReady(const cv::Mat &);
private:
    void timerEvent(QTimerEvent * ev) {
        if (ev->timerId() != m_timer.timerId()) return;
        cv::Mat frame;
        if (!m_videoCapture->read(frame)) { // Blocks until a new frame is ready
            m_timer.stop();
            return;
        }
        emit matReady(frame);
    }
};

class Converter : public QObject {
    Q_OBJECT
    QBasicTimer m_timer;
    cv::Mat m_frame;
    bool m_processAll = true;
    static void matDeleter(void* mat) { delete static_cast<cv::Mat*>(mat); }
    void queue(const cv::Mat & frame) {
        if (!m_frame.empty()) qDebug() << "Converter dropped frame!";
        m_frame = frame;
        if (! m_timer.isActive()) m_timer.start(0, this);
    }
    void process(cv::Mat frame) {
        cv::resize(frame, frame, cv::Size(), 0.3, 0.3, cv::INTER_AREA);
        cv::cvtColor(frame, frame, CV_BGR2RGB);
        const QImage image(frame.data, frame.cols, frame.rows, frame.step,
                           QImage::Format_RGB888, &matDeleter, new cv::Mat(frame));
        Q_ASSERT(image.constBits() == frame.data);
        emit imageReady(image);
    }
    void timerEvent(QTimerEvent * ev) {
        if (ev->timerId() != m_timer.timerId()) return;
        process(m_frame);
        m_frame.release();
        m_timer.stop();
    }
public:
    explicit Converter(QObject * parent = nullptr) : QObject(parent) {}
    void setProcessAll(bool all) { m_processAll = all; }
    Q_SIGNAL void imageReady(const QImage &);
    Q_SLOT void processFrame(const cv::Mat & frame) {
        if (m_processAll) process(frame); else queue(frame);
    }
};

class ImageViewer : public QWidget {
    Q_OBJECT
    QImage m_img;
    void paintEvent(QPaintEvent *) {
        QPainter p(this);
        p.drawImage(0, 0, m_img);
        m_img = {};
    }
public:
    ImageViewer(QWidget * parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_OpaquePaintEvent);
    }
    Q_SLOT void setImage(const QImage & img) {
        if (!m_img.isNull()) qDebug() << "Viewer dropped frame!";
        m_img = img;
        if (m_img.size() != size()) setFixedSize(m_img.size());
        update();
    }
};

class Thread final : public QThread { public: ~Thread() { quit(); wait(); } };

int main(int argc, char *argv[])
{
    qRegisterMetaType<cv::Mat>();
    QApplication app(argc, argv);
    ImageViewer view;
    Capture capture;
    Converter converter;
    Thread captureThread, converterThread;
    // Everything runs at the same priority as the gui, so it won't supply useless frames.
    converter.setProcessAll(false);
    captureThread.start();
    converterThread.start();
    capture.moveToThread(&captureThread);
    converter.moveToThread(&converterThread);
    QObject::connect(&capture, &Capture::matReady, &converter, &Converter::processFrame);
    QObject::connect(&converter, &Converter::imageReady, &view, &ImageViewer::setImage);
    view.show();
    QObject::connect(&capture, &Capture::started, [](){ qDebug() << "capture started"; });
    QMetaObject::invokeMethod(&capture, "start");
    return app.exec();
}

#include "main.moc"
