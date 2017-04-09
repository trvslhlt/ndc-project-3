
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

void ChatDialog::setupNet() {
	if (!sock->bind()) {
		exit(1);
	}
	// qDebug() << "myPortNo" << sock->myPortNo;
	// for (int i = 0; i < sock->myNeighbors.length(); i++) {
	// 	qDebug() << "myNeighbors" << sock->myNeighbors.at(i);
	// }
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
				qDebug() << inMap[SEQ_NO] << inMap[ORIGIN] << "RECVD";
			}
			// Make sure the received message is continuous
			if (inMap.contains(SEQ_NO) && ((inMap[SEQ_NO].toInt() == 1 && !sock->myStatus.contains(inMap[ORIGIN].toString())) || inMap[ORIGIN].toInt() - 1 == sock->myStatus[inMap["Origin"].toString()].toInt())) {
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
				sock->myNeighborsStatus[QString::number(disPort)] = 1;

				forwardMessage(datagram, &disPort, seqNo + "@" + origin);
			}
			sendACKDatagram(&disAddr, &disPort);
			// receiving && sending status
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

				// Check if it was a sender or a receiver at first palce
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
	// qDebug() << "sock->myData[messageID]" << sock->myData[messageID];
	// qDebug() << "sock->myStatus[sock->originName]" << sock->myStatus[sock->originName];

	qDebug() << messageID << sock->myData[messageID] << "YYY";

	// Send original message to a random picked neighor
	createOriginMessage(textline->text());

	// Clear the textline to get ready for the next input message.
	textline->clear();
}

void ChatDialog::createOriginMessage(QString text) {
	QByteArray datagram;
	QMap<QString, QVariant> map;
	QVariant v1(text);
	QVariant v2(sock->seqNo++);
	QVariant v3(sock->originName);
	map[CHAT_TEXT] = v1;
	map[SEQ_NO] = v2;
	map[ORIGIN] = v3;

	QDataStream * stream = new QDataStream(&datagram, QIODevice::WriteOnly);
	(*stream) << map;
	delete stream;

	int i = qrand() % sock->myNeighbors.length();

	QString messageID = v2.toString() + "@" + v3.toString();

	sock->writeDatagram(datagram.data(), datagram.size(),
	QHostAddress::LocalHost, sock->myNeighbors.at(i));
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
	QByteArray datagram;
	QMap<QString, QVariant> map;
	QVariant v1(content);
	QVariant v2(seqNo);
	QVariant v3(originName);
	map[CHAT_TEXT] = v1;
	map[SEQ_NO] = v2;
	map[ORIGIN] = v3;

	QDataStream * stream = new QDataStream(&datagram, QIODevice::WriteOnly);
	(*stream) << map;
	delete stream;

	sock->writeDatagram(datagram.data(), datagram.size(), *disAddr, *disPort);

	sock->myNeighborsStatus[QString::number(*disPort)] = 2;

	QString messageID = v2.toString() + "@" + v3.toString();
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
	QMap<QString, QVariant> map;
	QString messageID = sock->MyNeighborsLastMessage[disPort];

	QVariant v1(sock->myData[messageID].toString());
	QVariant v2(messageID.split("@")[0]);
	QVariant v3(messageID.split("@")[1]);
	map[CHAT_TEXT] = v1;
	map[SEQ_NO] = v2.toInt();
	map[ORIGIN] = v3;

	qDebug() << messageID << v1 << "RRR";

	QDataStream * stream = new QDataStream(&datagram, QIODevice::WriteOnly);
	(*stream) << map;
	delete stream;

	sock->writeDatagram(datagram.data(), datagram.size(), QHostAddress::LocalHost, disPort.toInt());
}

void ChatDialog::reForwardMessage(QString messageID, quint16 *disPort) {
	QByteArray datagram;
	QMap<QString, QVariant> map;

	QVariant v1(sock->myData[messageID].toString());
	QVariant v2(messageID.split("@")[0]);
	QVariant v3(messageID.split("@")[1]);
	map[CHAT_TEXT] = v1;
	map[SEQ_NO] = v2.toInt();
	map[ORIGIN] = v3;

	qDebug() << messageID << v1 << "ZZZ";

	QDataStream * stream = new QDataStream(&datagram, QIODevice::WriteOnly);
	(*stream) << map;
	delete stream;

	forwardMessage(datagram, disPort, messageID);
}

void ChatDialog::forwardMessage(QByteArray datagram, quint16 *disPort, QString messageID) {
	// QMap<QString, QVariant> map;
	// QDataStream * stream = new QDataStream(&datagram, QIODevice::WriteOnly);
	// (*stream) << map;
	// delete stream;

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

int main(int argc, char **argv) {
	QApplication app(argc,argv); // Initialize Qt toolkit
	ChatDialog dialog; // Create an initial chat dialog window
	dialog.show();
	dialog.setupNet();
	return app.exec(); // Enter the Qt main loop; everything else is event driven
}
