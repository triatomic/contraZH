// WBQtMessageBox.cpp -- see WBQtMessageBox.h. Maps the Win32 MB_* button/icon flags onto a
// QMessageBox parented to the Qt main window, and maps the clicked button back to the Win32
// ID* return code MFC's DoMessageBox contract expects.
#include "WBQtMessageBox.h"

#include <qt_windows.h>		// MB_*, ID* constants

#include <QAbstractButton>
#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <QStringList>

#include <string.h>

// The Qt main window (WBQtMainWindow*), C++ linkage, from WBQtBridge.cpp. Declared as QWidget*
// to avoid pulling the WBQtMainWindow header here.
QWidget *WBQt_MainWindowWidget(void);
QWidget *WBQt_DialogParent(void);

extern "C" int WBQtMessageBox_Show(const char *text, const char *caption, unsigned mbType)
{
	if (qApp == NULL)
	{
		return 0;
	}
	QWidget *parent = WBQt_MainWindowWidget();
	if (parent == NULL)
	{
		return 0;		// not inverted / no main window -- let the MFC default box run
	}

	QMessageBox box(parent);
	box.setText(QString::fromLocal8Bit((text != NULL) ? text : ""));
	box.setWindowTitle(QString::fromLocal8Bit((caption != NULL && caption[0] != 0)
		? caption : "WorldBuilder"));

	// Let callers embed clickable <a href> links in the text (e.g. the startup WARNING box's
	// GitHub link). Auto-detected rich text stays visually identical for plain messages, and
	// enabling the browser interaction flags makes any hyperlink open in the default browser.
	box.setTextFormat(Qt::AutoText);
	box.setTextInteractionFlags(Qt::TextBrowserInteraction);

	// Icon (MB_ICON* occupies bits 4-7).
	switch (mbType & MB_ICONMASK)
	{
		case MB_ICONHAND:			// == MB_ICONERROR / MB_ICONSTOP
			box.setIcon(QMessageBox::Critical);
			break;
		case MB_ICONQUESTION:
			box.setIcon(QMessageBox::Question);
			break;
		case MB_ICONEXCLAMATION:	// == MB_ICONWARNING
			box.setIcon(QMessageBox::Warning);
			break;
		case MB_ICONASTERISK:		// == MB_ICONINFORMATION
			box.setIcon(QMessageBox::Information);
			break;
		default:
			box.setIcon(QMessageBox::NoIcon);
			break;
	}

	// Buttons (MB_* button set occupies the low bits). Track each mapped button so the
	// clicked one can be turned back into the Win32 ID* code. QMessageBox::addButton returns
	// QPushButton*; hold them as QAbstractButton* to compare against clickedButton().
	QAbstractButton *bOk = NULL, *bCancel = NULL, *bYes = NULL, *bNo = NULL;
	QAbstractButton *bRetry = NULL, *bAbort = NULL, *bIgnore = NULL;
	switch (mbType & MB_TYPEMASK)
	{
		case MB_OKCANCEL:
			bOk = box.addButton(QMessageBox::Ok);
			bCancel = box.addButton(QMessageBox::Cancel);
			break;
		case MB_ABORTRETRYIGNORE:
			bAbort = box.addButton("Abort", QMessageBox::DestructiveRole);
			bRetry = box.addButton("Retry", QMessageBox::AcceptRole);
			bIgnore = box.addButton("Ignore", QMessageBox::RejectRole);
			break;
		case MB_YESNOCANCEL:
			bYes = box.addButton(QMessageBox::Yes);
			bNo = box.addButton(QMessageBox::No);
			bCancel = box.addButton(QMessageBox::Cancel);
			break;
		case MB_YESNO:
			bYes = box.addButton(QMessageBox::Yes);
			bNo = box.addButton(QMessageBox::No);
			break;
		case MB_RETRYCANCEL:
			bRetry = box.addButton(QMessageBox::Retry);
			bCancel = box.addButton(QMessageBox::Cancel);
			break;
		case MB_OK:
		default:
			bOk = box.addButton(QMessageBox::Ok);
			break;
	}

	// Default button (MB_DEFBUTTON2/3 pick the 2nd/3rd; default is the 1st).
	{
		QList<QAbstractButton *> buttons = box.buttons();
		int defIdx = 0;
		unsigned defFlag = mbType & MB_DEFMASK;
		if (defFlag == MB_DEFBUTTON2)
		{
			defIdx = 1;
		}
		else if (defFlag == MB_DEFBUTTON3)
		{
			defIdx = 2;
		}
		if (defIdx < buttons.size())
		{
			box.setDefaultButton(qobject_cast<QPushButton *>(buttons[defIdx]));
		}
	}

	box.exec();
	QAbstractButton *clicked = box.clickedButton();
	if (clicked == bOk)
	{
		return IDOK;
	}
	if (clicked == bCancel)
	{
		return IDCANCEL;
	}
	if (clicked == bYes)
	{
		return IDYES;
	}
	if (clicked == bNo)
	{
		return IDNO;
	}
	if (clicked == bRetry)
	{
		return IDRETRY;
	}
	if (clicked == bAbort)
	{
		return IDABORT;
	}
	if (clicked == bIgnore)
	{
		return IDIGNORE;
	}
	// Closed via the title-bar X / Esc: MFC returns IDCANCEL when a Cancel exists, else IDOK.
	return (bCancel != NULL) ? IDCANCEL : IDOK;
}

extern "C" int WBQtFileDialog_Run(int save, const char *title, const char *filter, const char *defExt,
	const char *initialPath, char *outPath, int outSize)
{
	if (qApp == NULL || outPath == NULL || outSize <= 0)
	{
		return 0;
	}
	outPath[0] = 0;

	// "Desc|pattern|Desc|pattern||" -> "Desc (pattern)" entries.
	QStringList filters;
	QStringList parts = QString::fromLocal8Bit((filter != NULL) ? filter : "").split('|');
	for (int i = 0; i + 1 < parts.size(); i += 2)
	{
		QString desc = parts[i].trimmed();
		QString patterns = parts[i + 1].trimmed();
		if (desc.isEmpty() || patterns.isEmpty())
		{
			break;
		}
		patterns.replace(';', ' ');
		// Qt reads the patterns from the last parentheses, so a "(.scb)" in the text must go.
		int paren = desc.lastIndexOf('(');
		if (paren > 0 && desc.endsWith(')'))
		{
			desc = desc.left(paren).trimmed();
		}
		filters.append(QString("%1 (%2)").arg(desc, patterns));
	}

	QWidget *parent = QApplication::activeModalWidget();
	if (parent == NULL)
	{
		parent = QApplication::activeWindow();
	}
	if (parent == NULL)
	{
		parent = WBQt_DialogParent();
	}

	QFileDialog dlg(parent, QString::fromLocal8Bit((title != NULL) ? title : ""));
	dlg.setAcceptMode(save ? QFileDialog::AcceptSave : QFileDialog::AcceptOpen);
	dlg.setFileMode(save ? QFileDialog::AnyFile : QFileDialog::ExistingFile);
	if (!filters.isEmpty())
	{
		dlg.setNameFilters(filters);
	}
	if (defExt != NULL && defExt[0] != 0)
	{
		dlg.setDefaultSuffix(QString::fromLocal8Bit(defExt[0] == '.' ? defExt + 1 : defExt));
	}
	if (initialPath != NULL && initialPath[0] != 0)
	{
		dlg.selectFile(QDir::fromNativeSeparators(QString::fromLocal8Bit(initialPath)));
	}
	if (dlg.exec() != QDialog::Accepted || dlg.selectedFiles().isEmpty())
	{
		return 0;
	}

	QByteArray path = QDir::toNativeSeparators(dlg.selectedFiles().first()).toLocal8Bit();
	if (path.size() >= outSize)
	{
		WBQtMessageBox_Show("The selected path is too long.", NULL, MB_OK | MB_ICONERROR);
		return 0;
	}
	strcpy(outPath, path.constData());
	int index = filters.indexOf(dlg.selectedNameFilter());
	return (index >= 0) ? index + 1 : 1;
}
