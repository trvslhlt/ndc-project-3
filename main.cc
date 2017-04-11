#include <unistd.h>
#include <QVBoxLayout>
#include <QApplication>
#include <QDebug>
#include "main.hh"

#define SEQ_NO "SeqNo"
#define ORIGIN "Origin"
#define CHAT_TEXT "ChatText"
#define WANT "Want"

ChatDialog::ChatDialog() {
	setWindowTitle("P2Papp");
	textview = new QTextEdit(this);
	textview->setReadOnly(true);
	textline = new QLineEdit(this);
	QVBoxLayout *layout = new QVBoxLayout();
	layout->addWidget(textview);
	layout->addWidget(textline);
	setLayout(layout);

	sock = new NetSocket();

	connect(textline, SIGNAL(returnPressed()),
	this, SLOT(handleReturnPressed()));
}
void ChatDialog::sendMessage(QMap<QString, QVariant> messageMap, QHostAddress *destAddr, quint16 *destPort) {
	QByteArray datagram;
	QDataStream *stream = new QDataStream(&datagram, QIODevice::WriteOnly);
	(*stream) << messageMap;
	delete stream;
	sock->writeDatagram(datagram.data(), datagram.size(), *destAddr, *destPort);
}

void ChatDialog::setupNet() {
	if (!sock->bind()) {
		exit(1);
	}
	connect(sock, SIGNAL(readyRead()),
	this, SLOT(processPendingDatagrams()));
}

void ChatDialog::processPendingDatagrams() {
	while (sock->hasPendingDatagrams()) {
		QByteArray datagram;
		QHostAddress disAddr;
		quint16 disPort;

		datagram.resize(sock->pendingDatagramSize());
		sock->readDatagram(datagram.data(), datagram.size(), &disAddr, &disPort);
		QVariantMap inMap;
		QDataStream instream(&datagram, QIODevice::ReadOnly);
		instream >> inMap;

		qDebug() << "disPort" << disPort;
		int reactStatus = sock->myNeighborsStatus[QString::number(disPort)];
		qDebug() << "reactStatus" << reactStatus;

		MyTimer *timer = sock->myNeighborsTimer[QString::number(disPort)];
		if (!timer) {
			sock->myNeighborsTimer[QString::number(disPort)] = new MyTimer(sock, QString::number(disPort));
			timer = sock->myNeighborsTimer[QString::number(disPort)];
			qDebug() << "???";
		}
		timer->stop();
		qDebug() << "STOP!" << disPort;

		if (reactStatus == 0) { // Never received
			if (inMap.contains(SEQ_NO)) {
				sock->myNeighborsStatus[QString::number(disPort)] = 1;
				// Make sure the received message is continuous
				if ((inMap[SEQ_NO].toInt() == 1 && !sock->myStatus.contains(inMap[ORIGIN].toString())) || inMap[ORIGIN].toInt() - 1 == sock->myStatus[inMap["Origin"].toString()].toInt()) {
					QString content = inMap[CHAT_TEXT].toString();
					QString seqNo = inMap[SEQ_NO].toString();
					QString origin = inMap[ORIGIN].toString();
					textview->append(tr("Received No%2 datagram from %3: \"%1\"")
					.arg(content).arg(seqNo).arg(origin));

					// Change status
					QVariant v(inMap[SEQ_NO]);
					sock->myStatus[origin] = v;
					QString messageID = seqNo + "@" + origin;
					sock->myData[messageID] = content;

					forwardMessage(datagram, &disPort, seqNo + "@" + origin);
				}
				sendACKDatagram(&disAddr, &disPort);
			}
		} else { // reactStatus != 0
			if (reactStatus == 1 && inMap.contains(SEQ_NO)) {
				int multipleConditions = (inMap[SEQ_NO].toInt() == 1 && !sock->myStatus.contains(inMap[ORIGIN].toString())) || \
				  inMap[SEQ_NO].toInt() - 1 == sock->myStatus[inMap[ORIGIN].toString()].toInt();
				if (multipleConditions) {
					QString content = inMap[CHAT_TEXT].toString();
					QString seqNo = inMap[SEQ_NO].toString();
					QString origin = inMap[ORIGIN].toString();
					textview->append(tr("Received No%2 datagram from %3: \"%1\"")
					.arg(content).arg(seqNo).arg(origin));

					// Change status
					QVariant v(inMap[SEQ_NO]);
					sock->myStatus[origin] = v;
					QString messageID = seqNo + "@" + origin;
					sock->myData[messageID] = content;

					forwardMessage(datagram, &disPort, seqNo + "@" + origin);
				}
				sendACKDatagram(&disAddr, &disPort);
			} else if (inMap.contains(WANT)){
				// Deserialize inside map
				QMap<QString, QVariantMap> inMap2;
				QDataStream instream2(&datagram, QIODevice::ReadOnly);
				instream2 >> inMap2;
				QMap<QString, QVariant> want = inMap2[WANT];

				QMapIterator<QString, QVariant> i(want);
				bool sentMissingMessage = false;

				// Adding keys that the sender doesn't know
				QMapIterator<QString, QVariant> i2(sock->myStatus);
				while (i2.hasNext()) {
					i2.next();
					if (want.contains(i2.key())) {
						continue;
					}
					want[i2.key()] = 1;
				}

				// Compare the information
				while (i.hasNext()) {
					i.next();
					qDebug() << "CHECK" << i.key() << sock->myStatus[i.key()].toInt() << i.value().toInt();
					if (sock->myStatus[i.key()].toInt() + 1 > i.value().toInt()) {
						QString messageID = QString::number(i.value().toInt()) + "@" + i.key();
						QString content = sock->myData[messageID].toString();
						sendMissingMessage(content, i.value().toInt(), i.key(), &disAddr, &disPort);
						sentMissingMessage = true;
						break;
					}
					if (sock->myStatus[i.key()].toInt() + 1 < i.value().toInt()) {
						break;
					}
				}

				if (sentMissingMessage) {
					continue;
				}

				// Send the status to receiver so the receiver knows when to change status
				// Flip a coin to forward message
				if (reactStatus == 2) {
					sendACKDatagram(&disAddr, &disPort);
				}

				// Check if it was a sender or a receiver in the first palce
				if (sock->myNeighborsOriginalMessage.contains(QString::number(disPort))) {
					qDebug() << "FORWARD";
					if (qrand() % 2 == 1) {
						qDebug() << "FORWARD!!!";
						// Store the first message sender sent, so that could forward the correct message
						reForwardMessage(sock->myNeighborsOriginalMessage[QString::number(disPort)], &disPort);
						sock->myNeighborsOriginalMessage.remove(QString::number(disPort));
					}
				}
				sock->myNeighborsStatus[QString::number(disPort)] = 0;
				qDebug() << "ZERO" << QString::number(disPort);
			}
		}
	}
}

void ChatDialog::handleReturnPressed() {
	qDebug() << "FIX: send message to other peers: " << textline->text();
	textview->append(tr("Received No%2 datagram from %3: \"%1\"")
		.arg(textline->text()).arg(sock->seqNo).arg(sock->originName));

	// Change self-status
	QVariant v(sock->seqNo);
	sock->myStatus[sock->originName] = v;
	QString messageID = v.toString() + "@" + sock->originName;
	qDebug() << sock->seqNo << "|||" << messageID;
	sock->myData[messageID] = textline->text();
	qDebug() << messageID << sock->myData[messageID] << "YYY";

	// Send original message to a random picked neighor
	createOriginMessage(textline->text());
	textline->clear();
}

void ChatDialog::createOriginMessage(QString text) {
	QMap<QString, QVariant> map = marshalRumor(text, QString::number(sock->seqNo++), sock->originName);
	int i = qrand() % sock->myNeighbors.length();
	QHostAddress destAddr = QHostAddress::LocalHost;
	quint16 destPort = sock->myNeighbors.at(i);
	sendMessage(map, &destAddr, &destPort);

	QString messageID = map[SEQ_NO].toString() + "@" + map[ORIGIN].toString();

	qDebug() << sock->myNeighbors.at(i);
	sock->myNeighborsStatus[QString::number(sock->myNeighbors.at(i))] = 2;
	sock->myNeighborsOriginalMessage[QString::number(sock->myNeighbors.at(i))] = messageID;

	MyTimer *timer = sock->myNeighborsTimer[QString::number(sock->myNeighbors.at(i))];
	if (!timer) {
		sock->myNeighborsTimer[QString::number(sock->myNeighbors.at(i))] = new MyTimer(sock, QString::number(sock->myNeighbors.at(i)));
		timer = sock->myNeighborsTimer[QString::number(sock->myNeighbors.at(i))];
	}
	sock->MyNeighborsLastMessage[QString::number(sock->myNeighbors.at(i))] = messageID;
	timer->start(1000);
	qDebug() << "START!" << sock->myNeighbors.at(i);
}

void ChatDialog::sendMissingMessage(QString content, int seqNo, QString originName, QHostAddress *disAddr, quint16 *disPort) {
	QMap<QString, QVariant> map = marshalRumor(content, QString::number(seqNo), originName);
  sendMessage(map, disAddr, disPort);

	sock->myNeighborsStatus[QString::number(*disPort)] = 2;

	QString messageID = map[SEQ_NO].toString() + "@" + map[ORIGIN].toString();
	MyTimer *timer = sock->myNeighborsTimer[QString::number(*disPort)];
	if (!timer) {
		sock->myNeighborsTimer[QString::number(*disPort)] = new MyTimer(sock, QString::number(*disPort));
		timer = sock->myNeighborsTimer[QString::number(*disPort)];
	}
	sock->MyNeighborsLastMessage[QString::number(*disPort)] = messageID;
	timer->start(1000);
}

void ChatDialog::sendACKDatagram(QHostAddress *disAddr, quint16 *disPort) {
	QByteArray datagram;
	QMap<QString, QMap<QString, QVariant> > map;
	QMap<QString, QVariant> status = sock->myStatus;
	QMapIterator<QString, QVariant> i(status);
	while (i.hasNext()) {
		i.next();
		status[i.key()] = i.value().toInt() + 1;
	}
	map[WANT] = status;
	qDebug() << "ACK" << map;

	QDataStream * stream = new QDataStream(&datagram, QIODevice::WriteOnly);
	(*stream) << map;
	delete stream;

	sock->writeDatagram(datagram.data(), datagram.size(), *disAddr, *disPort);
	sock->myNeighborsStatus[QString::number(*disPort)] = 1;
}

void MyTimer::resendLostMessage() {
	QByteArray datagram;
	QString messageID = sock->MyNeighborsLastMessage[disPort];
	QString text = sock->myData[messageID].toString();
	QString seqNo = messageID.split("@")[0];
	QString originName = messageID.split("@")[1];
	QMap<QString, QVariant> map = marshalRumor(text, seqNo, originName);
	QDataStream *stream = new QDataStream(&datagram, QIODevice::WriteOnly);
	(*stream) << map;
	qDebug() << messageID << text << "RRR";
	delete stream;

	sock->writeDatagram(datagram.data(), datagram.size(), QHostAddress::LocalHost, disPort.toInt());
}

void ChatDialog::reForwardMessage(QString messageID, quint16 *disPort) {
	QByteArray datagram;
	QString text = sock->myData[messageID].toString();
	QString seqNo = messageID.split("@")[0];
	QString originName = messageID.split("@")[1];
	QMap<QString, QVariant> map = marshalRumor(text, seqNo, originName);
	QDataStream *stream = new QDataStream(&datagram, QIODevice::WriteOnly);
	(*stream) << map;
	delete stream;
	qDebug() << messageID << map[CHAT_TEXT] << "ZZZ";

	forwardMessage(datagram, disPort, messageID);
}

void ChatDialog::forwardMessage(QByteArray datagram, quint16 *disPort, QString messageID) {
	if (sock->myNeighbors.length() == 1) return;
	int i = qrand() % sock->myNeighbors.length();
	while (sock->myNeighbors.at(i) == *disPort) {
		i = qrand() % sock->myNeighbors.length();
	}
	sock->writeDatagram(datagram.data(), datagram.size(), QHostAddress::LocalHost, sock->myNeighbors.at(i));
	sock->myNeighborsStatus[QString::number(sock->myNeighbors.at(i))] = 2;
	sock->myNeighborsOriginalMessage[QString::number(sock->myNeighbors.at(i))] = messageID;

	MyTimer *timer = sock->myNeighborsTimer[QString::number(sock->myNeighbors.at(i))];
	if (!timer) {
		sock->myNeighborsTimer[QString::number(sock->myNeighbors.at(i))] = new MyTimer(sock, QString::number(sock->myNeighbors.at(i)));
		timer = sock->myNeighborsTimer[QString::number(sock->myNeighbors.at(i))];
	}
	sock->MyNeighborsLastMessage[QString::number(sock->myNeighbors.at(i))] = messageID;
	timer->start(1000);
}

void ChatDialog::configureAntiEntropy() {
  qDebug() << "configureAntiEntropy";
	antientropyTimer = new AntientropyTimer(10000);
}

void AntientropyTimer::didTimeoutAntientropy() {
  qDebug() << "didTimeoutAntientropy";
}

QMap<QString, QVariant> marshalRumor(QString text, QString seqNo, QString originName) {
	QMap<QString, QVariant> map;
	QVariant v1(text);
	QVariant v2(seqNo);
	QVariant v3(originName);
	map[CHAT_TEXT] = v1;
	map[SEQ_NO] = v2;
	map[ORIGIN] = v3;
	return map;
}

int main(int argc, char **argv) {
	QApplication app(argc,argv); // Initialize Qt toolkit
	ChatDialog dialog; // Create an initial chat dialog window
	dialog.show();
	dialog.setupNet();
	dialog.configureAntiEntropy();
	return app.exec(); // Enter the Qt main loop; everything else is event driven
}
