#ifndef NETSOCKET_HH
#define NETSOCKET_HH

#include <QUdpSocket>
#include <QHostInfo>
#include <QUuid>

class NetSocket : public QUdpSocket
{
	Q_OBJECT

public:
	NetSocket();

	// Bind this socket to a P2Papp-specific default port.
	bool bind();

	int myPortMin, myPortMax;
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


private:
	// int myPortMin, myPortMax;
};

#endif // NETSOCKET_HH