// WBQtMapIniReportPrivate.h -- the QDialog for the map.ini report viewer. Separate from the
// public WBQtMapIniReport.h (a pure C facade) so this Qt header is only ever seen by the Qt
// TU (WBQtMapIniReport.cpp), and AUTOMOC generates the moc the standard header-class way.
#ifndef WB_QT_MAPINI_REPORT_PRIVATE_H
#define WB_QT_MAPINI_REPORT_PRIVATE_H

#include <QDialog>
#include <QFrame>
#include <QList>
#include <QStringList>
#include <QString>

class QLabel;
class QMouseEvent;
class QPlainTextEdit;
class QToolButton;
class QVBoxLayout;

namespace Ui { class WBQtMapIniReportDialog; }	// generated from WBQtMapIniReportDialog.ui

// One map.ini block as the loader reported it.
struct WBQtMapIniBlockData
{
	QString store;			///< block keyword: Object, Weapon, ParticleSystem, ...
	QString name;
	int status;				///< WBQT_MAPINI_BLOCK_*
	int firstLine;			///< 1-based line in map.ini
	QString source;			///< verbatim block text
	QList<int> blanked;		///< 0-based source lines the loader blanked out
};

// A block's collapsible card: a header (toggle, title, status, line, copy, pop-out) over a
// read-only code view.
class WBQtMapIniBlockCard : public QFrame
{
	Q_OBJECT
public:
	WBQtMapIniBlockCard(const WBQtMapIniBlockData &data, QWidget *parent);

	void setExpanded(bool expanded);
	bool matches(const QString &needle) const;
	bool titleMatches(const QString &needle) const;

protected:
	virtual void mousePressEvent(QMouseEvent *event);

private slots:
	void onToggle();
	void onCopy();
	void onPopOut();

private:
	WBQtMapIniBlockData m_data;
	QToolButton *m_toggle;
	QPlainTextEdit *m_code;
};

class WBQtMapIniReportDialog : public QDialog
{
	Q_OBJECT
public:
	// applyMode true -> OK/Cancel (caller applies on OK); false -> a single Close button.
	WBQtMapIniReportDialog(const QString &title, const QString &text, bool applyMode,
		const QStringList &summary, const QList<WBQtMapIniBlockData> &blocks,
		QWidget *parent = 0);
	virtual ~WBQtMapIniReportDialog();

	void addMessage(int kind, const QString &title, const QString &body);

private slots:
	void onFilterChanged(const QString &text);
	void onExpandAll();
	void onCollapseAll();
	void onCopy();

private:
	struct Group
	{
		QLabel *header;
		QList<WBQtMapIniBlockCard *> cards;
	};

	void buildSummary(const QStringList &summary);
	void buildBlocks(const QList<WBQtMapIniBlockData> &blocks);

	Ui::WBQtMapIniReportDialog *m_ui;	// owns the static widget tree (WBQtMapIniReportDialog.ui)

	QVBoxLayout *m_content;		///< the scroll area's column of panels
	int m_messageInsertAt;		///< where the next message panel goes, above the blocks
	QList<Group> m_groups;
	QString m_rawText;			///< the full plain-text report, for Copy
};

#endif // WB_QT_MAPINI_REPORT_PRIVATE_H
