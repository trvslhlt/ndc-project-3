#ifndef NETSOCKET_HH
#define NETSOCKET_HH

#include <QUdpSocket>
#include <QHostInfo>
#include <QUuid>
#include <QTimer>

class NetSocket;

class AntientropyTimer : public QObject {
	Q_OBJECT
public:
	AntientropyTimer(int interval_, NetSocket *sock_);
	void stop();
private:
	int interval;
	QTimer *timer;
	NetSocket *sock;
public slots:
	void didTimeoutAntientropy();
};

class MyTimer : public QObject {
	Q_OBJECT

public:
	MyTimer(NetSocket *sock_, QString disPort_) {
		sock = sock_;
		disPort = disPort_;
		timer = new QTimer;
		connect(timer, SIGNAL(timeout()), this, SLOT(resendLostMessage()));
	}
	QTimer *timer;
	QString disPort;
	NetSocket *sock;
	void start(int msec) {timer->start(msec);}
	void stop() {timer->stop();}
public slots:
	void resendLostMessage();
};

class NetSocket : public QUdpSocket {
	Q_OBJECT

public:
	NetSocket();

	// Bind this socket to a P2Papp-specific default port.
	bool bind();

	int myPortNo;
	QList<int> myNeighbors;
	// Define on the send/receive status of the neighors
	// 0 init 1 receiving 2 sending
	QMap<QString, int> myNeighborsStatus;
	QMap<QString, QString> myNeighborsOriginalMessage;
	QString originName;
	quint32 seqNo;
	QMap<QString, QVariant> myData;
	QMap<QString, QVariant> myStatus;

	QMap<QString, MyTimer *> myNeighborsTimer;
	QMap<QString, QString> MyNeighborsLastMessage;


private:
	int myPortMin, myPortMax;
};

#endif // NETSOCKET_HH
