
#include <unistd.h>

#include <QVBoxLayout>
#include <QApplication>
#include <QDebug>

#include "main.hh"

ChatDialog::ChatDialog()
{
	setWindowTitle("P2Papp");

	// Read-only text box where we display messages from everyone.
	// This widget expands both horizontally and vertically.
	textview = new QTextEdit(this);
	textview->setReadOnly(true);

	// Small text-entry box the user can enter messages.
	// This widget normally expands only horizontally,
	// leaving extra vertical space for the textview widget.
	//
	// You might change this into a read/write QTextEdit,
	// so that the user can easily enter multi-line messages.
	textline = new QLineEdit(this);

	// Lay out the widgets to appear in the main window.
	// For Qt widget and layout concepts see:
	// http://doc.qt.nokia.com/4.7-snapshot/widgets-and-layouts.html
	QVBoxLayout *layout = new QVBoxLayout();
	layout->addWidget(textview);
	layout->addWidget(textline);
	setLayout(layout);
	sock = new NetSocket();

	// Register a callback on the textline's returnPressed signal
	// so that we can send the message entered by the user.
	connect(textline, SIGNAL(returnPressed()),
		this, SLOT(gotReturnPressed()));
}

void ChatDialog::setupNet()
{	
	if (!sock->bind())
		exit(1);
    connect(sock, SIGNAL(readyRead()),
            this, SLOT(processPendingDatagrams()));	
}

void ChatDialog::processPendingDatagrams()
{
    while (sock->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(sock->pendingDatagramSize());
        sock->readDatagram(datagram.data(), datagram.size());
        QVariantMap inMap;
		QDataStream instream(&datagram, QIODevice::ReadOnly);
		instream >> inMap;
		QString con = inMap["ChatText"].toString();
        textview->append(tr("Received datagram: \"%1\"")
                             .arg(con));
    }
}

void ChatDialog::gotReturnPressed()
{
	// Initially, just echo the string locally.
	// Insert some networking code here...
	qDebug() << "FIX: send message to other peers: " << textline->text();
	// textview->append(textline->text());

	// Clear the textline to get ready for the next input message.
	sendDatagram(textline->text());
	textline->clear();
}

void ChatDialog::sendDatagram(QString text)
{
    QByteArray datagram;
    QMap<QString, QVariant> map;
    QVariant v(text);
    map["ChatText"] = v;

	QDataStream * stream = new QDataStream(&datagram, QIODevice::WriteOnly);
	(*stream) << map;
	delete stream;

    for (int p = sock->myPortMin; p <= sock->myPortMax; p++) {
		sock->writeDatagram(datagram.data(), datagram.size(),
                             QHostAddress::LocalHost, p);
	}
}

int main(int argc, char **argv)
{
	// Initialize Qt toolkit
	QApplication app(argc,argv);
	// Create an initial chat dialog window
	ChatDialog dialog;

	dialog.show();
	dialog.setupNet();

	// Create a UDP network socket
	// NetSocket sock;
	// if (!sock.bind())
	// 	exit(1);

	// Enter the Qt main loop; everything else is event driven
	return app.exec();
}

