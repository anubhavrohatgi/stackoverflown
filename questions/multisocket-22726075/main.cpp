// https://github.com/KubaO/stackoverflown/tree/master/questions/multisocket-22726075
#include <QtNetwork>

class EchoServer : public QTcpServer
{
   QStack<QTcpSocket*> m_pool;
   void incomingConnection(qintptr descr) Q_DECL_OVERRIDE {
      if (m_pool.isEmpty()) {
         auto s = new QTcpSocket(this);
         QObject::connect(s, &QTcpSocket::readyRead, s, [s]{
            s->write(s->readAll());
         });
         QObject::connect(s, &QTcpSocket::disconnected, this, [this, s]{
            m_pool.push(s);
         });
         m_pool.push(s);
      }
      m_pool.pop()->setSocketDescriptor(descr, QTcpSocket::ConnectedState);
   }
public:
   ~EchoServer() { qDebug() << "pool size:" << m_pool.size(); }
};

void setupEchoClient(QTcpSocket & sock)
{
   static const char kByteCount[] = "byteCount";
   QObject::connect(&sock, &QTcpSocket::connected, [&sock]{
      auto byteCount = 64 + qrand() % 65536;
      sock.setProperty(kByteCount, byteCount);
      sock.write(QByteArray(byteCount, '\x2A'));
   });
   QObject::connect(&sock, &QTcpSocket::readyRead, [&sock]{
      auto byteCount = sock.property(kByteCount).toInt();
      if (byteCount) {
         auto read = sock.read(sock.bytesAvailable()).size();
         byteCount -= read;
      }
      if (byteCount <= 0) sock.disconnectFromHost();
      sock.setProperty(kByteCount, byteCount);
   });
}

int main(int argc, char *argv[])
{
   QCoreApplication a(argc, argv);
   QHostAddress addr("127.0.0.1");
   quint16 port = 5050;

   EchoServer server;
   if (! server.listen(addr, port)) qFatal("can't listen");

   QTcpSocket clientSocket;
   setupEchoClient(clientSocket);

   auto connectsLeft = 20;
   auto connector = [&clientSocket, &addr, port, &connectsLeft]{
      if (connectsLeft--) {
         qDebug() << "connecting" << connectsLeft;
         clientSocket.connectToHost(addr, port);
      } else
         qApp->quit();
   };
   // reconnect upon disconnection
   QObject::connect(&clientSocket, &QTcpSocket::disconnected, connector);
   // initiate first connection
   connector();

   return a.exec();
}
