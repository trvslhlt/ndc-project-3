#ifndef P2PAPP_MAIN_HH
#define P2PAPP_MAIN_HH

#include <QDialog>
#include <QTextEdit>
#include <QLineEdit>
#include <QUdpSocket>

#include "netsocket.hh"

class ChatDialog : public QDialog {
	Q_OBJECT

public:
	ChatDialog();
	void setupNet();

public slots:
	void handleReturnPressed();
	void processPendingDatagrams();

private:
	QTextEdit *textview;
	QLineEdit *textline;
	NetSocket *sock;
	void createOriginMessage(QString text);
	void sendMissingMessage(QString content, int seqNo, QString originName, QHostAddress *disAddr, quint16 *disPor);
	void sendACKDatagram(QHostAddress *disAddr, quint16 *disPort);
	void forwardMessage(QByteArray datagram, quint16 *disPort, QString messageID);
	void reForwardMessage(QString messageID, quint16 *disPort);
	void resendLostMessage(QString disPort);
};

QMap<QString, QVariant> marshalRumor(QString text, QString seqNo, QString originName);
//QMap<QString, QVariant> marshalStatus(QString text, QString seqNo, QString originName);


#endif // P2PAPP_MAIN_HH
