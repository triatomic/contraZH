// WBQtCondActDialog.h -- the Qt condition/action picker; run it via WBQtCondAct_Run().
#ifndef WB_QT_CONDACT_DIALOG_H
#define WB_QT_CONDACT_DIALOG_H

#include <QDialog>
#include <QHash>
#include <QList>
#include <QStringList>

class QButtonGroup;
class QLayout;
class QTreeWidgetItem;

namespace Ui { class WBQtCondActDialog; }	// generated from WBQtCondActDialog.ui

// The template list beside the item's sentence, whose parameters are chips.
class WBQtCondActDialog : public QDialog
{
	Q_OBJECT
public:
	WBQtCondActDialog(void *item, bool isAction, QWidget *parent = 0);
	virtual ~WBQtCondActDialog();

	// Records the chosen type under Recent.
	virtual void accept();

protected:
	bool eventFilter(QObject *watched, QEvent *event);

private slots:
	void onFilterChanged(const QString &text);
	void onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
	void onChipClicked();
	void onTreeContextMenu(const QPoint &pos);
	void onItemDoubleClicked(QTreeWidgetItem *item, int column);
	void onScopeClicked(int scope);
	void onNotesToggled(bool open);

private:
	void buildTree(const QString &filter);
	// Both return the row for curType, or NULL.
	QTreeWidgetItem *buildCatalog(int curType);
	QTreeWidgetItem *buildFlat(const QList<int> &types, bool rank, const QStringList &words, int curType);
	bool matchesWords(int type, const QStringList &words) const;
	QTreeWidgetItem *makeLeaf(QTreeWidgetItem *parent, int kind, const QString &label, int type, bool favorite);
	void toggleFavorite(int type);
	void updateScopeLabels();
	void updateCountLabel(int count);
	void selectCurrentType(QTreeWidgetItem *leaf);
	void selectFirstMatch();
	void renderSentence();
	void updateWarnings();
	void showHelpForType(int type);
	void applyTreeFont();
	QStringList savedList(bool favorites) const;
	void setSavedList(bool favorites, const QStringList &paths);

	Ui::WBQtCondActDialog *m_ui;	// owns the static widget tree (WBQtCondActDialog.ui)

	void *m_item;
	int m_isAction;
	bool m_updating;
	QLayout *m_flow;				///< the sentence's words and chips
	QButtonGroup *m_scope;			///< All / Favorites / Recent
	QStringList m_names;			///< template index -> '/'-separated path
	QStringList m_names2;			///< template index -> alternate path or ""
	QStringList m_families;			///< template index -> its parameter families, one QChar each
	QHash<QString, int> m_pathIndex;	///< template path -> template index
};

#endif // WB_QT_CONDACT_DIALOG_H
