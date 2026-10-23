// WBQtCondActDialogClassic.h -- the original condition/action picker, shown while the script editor's "New design" is off.
#ifndef WB_QT_CONDACT_DIALOG_CLASSIC_H
#define WB_QT_CONDACT_DIALOG_CLASSIC_H

#include <QDialog>

class QCheckBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTextBrowser;
class QTreeWidget;
class QTreeWidgetItem;
class QUrl;

namespace Ui { class WBQtCondActDialogClassic; }	// generated from WBQtCondActDialogClassic.ui

class WBQtCondActDialogClassic : public QDialog
{
	Q_OBJECT
public:
	WBQtCondActDialogClassic(void *item, bool isAction, QWidget *parent = 0);
	virtual ~WBQtCondActDialogClassic();

protected:
	bool eventFilter(QObject *watched, QEvent *event);

private slots:
	void onSearchLive(const QString &text);	// NewSearch: live filter, no beep / no message box
	void onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
	void onLinkClicked(const QUrl &url);
	void onSearch();
	void onReset();
	void onCompressToggled(bool checked);

private:
	void populateTree();
	int buildTree(const QString &filter);	// build the category tree; filter empty => full list
	void applyFilter(const QString &searchText, bool announce);	// shared by onSearch/onSearchLive
	void selectCurrentType(QTreeWidgetItem *leaf);
	void renderSentence();
	void updateWarnings();
	void showHelpForType(int type);
	void applyTreeFont();

	Ui::WBQtCondActDialogClassic *m_ui;	// owns the static widget tree (WBQtCondActDialogClassic.ui)

	void *m_item;
	int m_isAction;
	bool m_updating;

	QLineEdit *m_searchEdit;
	QCheckBox *m_compressCheck;
	QTreeWidget *m_tree;
	QTextBrowser *m_sentence;
	QGroupBox *m_warningsBox;
	QLabel *m_warningsLabel;
	QLabel *m_helpLabel;
};

#endif // WB_QT_CONDACT_DIALOG_CLASSIC_H
