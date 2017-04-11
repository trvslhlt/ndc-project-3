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
	void configureAntiEntropy();
	void sendACKDatagram(QHostAddress *destAddr, quint16 *destPort);
private:
	QTextEdit *textview;
	QLineEdit *textline;
	NetSocket *sock;
	AntientropyTimer *antientropyTimer;
	void createOriginMessage(QString text);
	void sendMissingMessage(QString content, int seqNo, QString originName, QHostAddress *disAddr, quint16 *disPor);
	void forwardMessage(QByteArray datagram, quint16 *disPort, QString messageID);
	void reForwardMessage(QString messageID, quint16 *disPort);
	void resendLostMessage(QString disPort);
	void sendMessage(QMap<QString, QVariant> messageMap, QHostAddress *destAddr, quint16 *destPort);
	void didTimeoutAntientropy();
};

QMap<QString, QVariant> marshalRumor(QString text, QString seqNo, QString originName);

#endif // P2PAPP_MAIN_HH
