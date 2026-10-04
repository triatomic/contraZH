// WBQtCondActDialog.cpp -- see WBQtCondActDialog.h.
#include "WBQtCondActDialog.h"
#include "ui_WBQtCondActDialog.h"
#include "WBQtCondActBridge.h"
#include "WBQtCondActDialogClassic.h"

// The script editor's "New design" setting (WBQtScriptBridge.cpp).
extern "C" int WBQtScript_GetNewDesign(void);

#include <QApplication>
#include <QButtonGroup>
#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QTreeWidget>
#include <QtAlgorithms>
#include <QtMath>

namespace
{
	const int kNameCap = 512;
	const int kTextCap = 1024;
	const int kBigCap = 4096;
	const int kRecentMax = 10;
	const int kSavedListCap = 65536;

	// Item data roles: the template index (-1 on folders), the row kind, a folder's leaf count, a
	// flat row's category path, the template's parameter families, and its favorite flag.
	// Families travel as a digit string: a QVariantList's nodes would come from WB's pooled
	// operator new and be freed by Qt5Core's CRT delete, corrupting the heap.
	const int kTemplateRole = Qt::UserRole;
	const int kKindRole = Qt::UserRole + 1;
	const int kCountRole = Qt::UserRole + 2;
	const int kCrumbRole = Qt::UserRole + 3;
	const int kFamiliesRole = Qt::UserRole + 4;
	const int kFavoriteRole = Qt::UserRole + 5;

	enum RowKind
	{
		kRowHeader,		///< a top-level category
		kRowFolder,
		kRowLeaf,
		kRowMatch,		///< a flat-list leaf with its category path under it
		kRowEmpty		///< the placeholder of an empty list
	};

	enum Scope
	{
		kScopeAll,
		kScopeFavorites,
		kScopeRecent
	};

	const int kStarWidth = 22;
	const int kDotStep = 13;
	const qreal kDotRadius = 4.5;

	QString templateName(int isAction, int i)
	{
		char buf[kNameCap];
		buf[0] = 0;
		WBQtCondActData_GetTemplateName(isAction, i, buf, sizeof(buf));
		return QString::fromLocal8Bit(buf);
	}

	QString templateName2(int isAction, int i)
	{
		char buf[kNameCap];
		buf[0] = 0;
		WBQtCondActData_GetTemplateName2(isAction, i, buf, sizeof(buf));
		return QString::fromLocal8Bit(buf);
	}

	QString templateHelp(int isAction, int i)
	{
		char buf[kBigCap];
		buf[0] = 0;
		WBQtCondActData_GetTemplateHelp(isAction, i, buf, sizeof(buf));
		// == ParseHelpText: the help strings carry literal "\n" escapes.
		return QString::fromLocal8Bit(buf).replace("\\n", "\n");
	}

	QColor familyColour(int family)
	{
		switch (family)
		{
			case WBQT_PARAM_THING:	return QColor(55, 105, 165);
			case WBQT_PARAM_PLAYER:	return QColor(160, 100, 40);
			case WBQT_PARAM_PLACE:	return QColor(55, 130, 75);
			case WBQT_PARAM_NUMBER:	return QColor(70, 110, 120);
			case WBQT_PARAM_TEXT:	return QColor(125, 85, 165);
			case WBQT_PARAM_LOGIC:	return QColor(140, 115, 35);
			default:				return QColor(95, 95, 95);
		}
	}

	const char *familyName(int family)
	{
		switch (family)
		{
			case WBQT_PARAM_THING:	return "Thing";
			case WBQT_PARAM_PLAYER:	return "Player";
			case WBQT_PARAM_PLACE:	return "Place";
			case WBQT_PARAM_NUMBER:	return "Number";
			case WBQT_PARAM_TEXT:	return "Text";
			case WBQT_PARAM_LOGIC:	return "Logic";
			default:				return "Other";
		}
	}

	QColor withAlpha(const QColor &colour, int alpha)
	{
		QColor c(colour);
		c.setAlpha(alpha);
		return c;
	}

	QFont smallFont(const QFont &font)
	{
		QFont small(font);
		if (small.pixelSize() > 0)
		{
			small.setPixelSize(small.pixelSize() - 2);
		}
		else
		{
			small.setPointSizeF(small.pointSizeF() - 1.5);
		}
		return small;
	}

	// The star's hit area at a row's right edge; the delegate and the click test share it.
	QRect starRect(const QRect &row)
	{
		return QRect(row.right() - kStarWidth - 1, row.top(), kStarWidth, row.height());
	}

	// Sorts folders before leaves, then by name.
	class CatalogItem : public QTreeWidgetItem
	{
	public:
		explicit CatalogItem(int kind)
		{
			setData(0, kKindRole, kind);
		}

		virtual bool operator<(const QTreeWidgetItem &other) const
		{
			const bool folder = data(0, kTemplateRole).toInt() < 0;
			const bool otherFolder = other.data(0, kTemplateRole).toInt() < 0;
			if (folder != otherFolder)
			{
				return folder;
			}
			return text(0).compare(other.text(0), Qt::CaseInsensitive) < 0;
		}
	};

	// A flat-list row ordered by how well its leaf label matches, then by name.
	struct RankedType
	{
		int score;
		QString key;
		int type;

		bool operator<(const RankedType &other) const
		{
			if (score != other.score)
			{
				return score > other.score;
			}
			return key.compare(other.key, Qt::CaseInsensitive) < 0;
		}
	};

	// Lays its widgets out left to right and wraps, centring each line's items vertically.
	class FlowLayout : public QLayout
	{
	public:
		FlowLayout(QWidget *parent, int spacing) : QLayout(parent), m_spacing(spacing)
		{
			setContentsMargins(8, 8, 8, 8);
		}

		virtual ~FlowLayout()
		{
			QLayoutItem *item;
			while ((item = takeAt(0)) != NULL)
			{
				delete item;
			}
		}

		virtual void addItem(QLayoutItem *item) { m_items.append(item); }
		virtual int count() const { return m_items.size(); }
		virtual QLayoutItem *itemAt(int index) const { return m_items.value(index); }
		virtual QLayoutItem *takeAt(int index)
		{
			return (index >= 0 && index < m_items.size()) ? m_items.takeAt(index) : NULL;
		}
		virtual Qt::Orientations expandingDirections() const { return 0; }
		virtual bool hasHeightForWidth() const { return true; }
		virtual int heightForWidth(int width) const { return doLayout(QRect(0, 0, width, 0), true); }
		virtual void setGeometry(const QRect &rect)
		{
			QLayout::setGeometry(rect);
			doLayout(rect, false);
		}
		virtual QSize sizeHint() const { return minimumSize(); }
		virtual QSize minimumSize() const
		{
			QSize size;
			for (int i = 0; i < m_items.size(); ++i)
			{
				size = size.expandedTo(m_items.at(i)->minimumSize());
			}
			int left, top, right, bottom;
			getContentsMargins(&left, &top, &right, &bottom);
			return size + QSize(left + right, top + bottom);
		}

	private:
		int doLayout(const QRect &rect, bool testOnly) const
		{
			int left, top, right, bottom;
			getContentsMargins(&left, &top, &right, &bottom);
			const QRect area = rect.adjusted(left, top, -right, -bottom);
			int x = area.x();
			int y = area.y();
			int lineHeight = 0;
			int lineStart = 0;
			for (int i = 0; i <= m_items.size(); ++i)
			{
				const bool last = (i == m_items.size());
				const QSize hint = last ? QSize() : m_items.at(i)->sizeHint();
				const bool wrap = !last && x > area.x() && x + hint.width() > area.right() + 1;
				if (last || wrap)
				{
					// Place the finished line, centred on its tallest item.
					if (!testOnly)
					{
						int lx = area.x();
						for (int j = lineStart; j < i; ++j)
						{
							const QSize h = m_items.at(j)->sizeHint();
							m_items.at(j)->setGeometry(QRect(QPoint(lx, y + (lineHeight - h.height()) / 2), h));
							lx += h.width() + m_spacing;
						}
					}
					if (last)
					{
						break;
					}
					x = area.x();
					y += lineHeight + m_spacing;
					lineHeight = 0;
					lineStart = i;
				}
				x += hint.width() + m_spacing;
				lineHeight = qMax(lineHeight, hint.height());
			}
			return y + lineHeight - rect.y() + bottom;
		}

		QList<QLayoutItem *> m_items;
		int m_spacing;
	};

	// Paints every list row: category headers with counts, folders with chevrons, and leaves with
	// a bold lead, filter highlights, parameter dots and a favorite star.
	class RowDelegate : public QStyledItemDelegate
	{
	public:
		explicit RowDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

		void setWords(const QStringList &words) { m_words = words; }

		virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
		{
			const int lineHeight = QFontMetrics(option.font).height();
			switch (index.data(kKindRole).toInt())
			{
				case kRowHeader:	return QSize(100, lineHeight + 16);
				case kRowMatch:		return QSize(100, lineHeight + QFontMetrics(smallFont(option.font)).height() + 12);
				default:			return QSize(100, lineHeight + 10);
			}
		}

		virtual void paint(QPainter *painter, const QStyleOptionViewItem &option,
			const QModelIndex &index) const
		{
			const int kind = index.data(kKindRole).toInt();
			const QColor text = option.palette.color(QPalette::Text);
			const QColor dim = withAlpha(text, 150);
			const QRect row = option.rect.adjusted(2, 1, -2, -1);
			const bool selected = (option.state & QStyle::State_Selected) != 0;
			const bool hover = (option.state & QStyle::State_MouseOver) != 0;

			painter->save();
			painter->setRenderHint(QPainter::Antialiasing, true);
			painter->setPen(Qt::NoPen);
			if (selected && kind != kRowEmpty)
			{
				const QColor accent = option.palette.color(QPalette::Highlight);
				painter->setBrush(withAlpha(accent, 110));
				painter->drawRoundedRect(row, 5, 5);
				painter->fillRect(QRect(row.left(), row.top() + 3, 3, row.height() - 6), accent.lighter(130));
			}
			else if (hover && kind != kRowEmpty)
			{
				painter->setBrush(withAlpha(text, 18));
				painter->drawRoundedRect(row, 5, 5);
			}

			QRect area = row.adjusted(8, 0, -6, 0);
			if (kind == kRowEmpty)
			{
				QFont italic(option.font);
				italic.setItalic(true);
				painter->setFont(italic);
				painter->setPen(dim);
				painter->drawText(area, Qt::AlignLeft | Qt::AlignVCenter, index.data(Qt::DisplayRole).toString());
			}
			else if (kind == kRowHeader || kind == kRowFolder)
			{
				paintFolder(painter, option, index, area, kind == kRowHeader, text, dim);
			}
			else
			{
				paintLeaf(painter, option, index, area, kind == kRowMatch, hover, text, dim);
			}
			painter->restore();
		}

	private:
		void paintFolder(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index,
			QRect area, bool header, const QColor &text, const QColor &dim) const
		{
			const QTreeView *view = qobject_cast<const QTreeView *>(option.widget);
			const bool open = view != NULL && view->isExpanded(index);

			// The chevron points right when closed and down when open.
			const QPointF c(area.left() + 4, area.center().y() + 0.5);
			QPolygonF chevron;
			if (open)
			{
				chevron << QPointF(c.x() - 4, c.y() - 2) << QPointF(c.x() + 4, c.y() - 2) << QPointF(c.x(), c.y() + 3);
			}
			else
			{
				chevron << QPointF(c.x() - 2, c.y() - 4) << QPointF(c.x() + 3, c.y()) << QPointF(c.x() - 2, c.y() + 4);
			}
			painter->setPen(Qt::NoPen);
			painter->setBrush(dim);
			painter->drawPolygon(chevron);
			area.setLeft(area.left() + 16);

			const QString count = QString::number(index.data(kCountRole).toInt());
			const QFont small = smallFont(option.font);
			const QFontMetrics smallMetrics(small);
			const int badgeWidth = smallMetrics.horizontalAdvance(count) + 14;
			const QRect badge(area.right() - badgeWidth + 1, area.center().y() - smallMetrics.height() / 2 - 1,
				badgeWidth, smallMetrics.height() + 2);
			painter->setFont(small);
			if (header)
			{
				painter->setBrush(withAlpha(text, 35));
				painter->drawRoundedRect(badge, badge.height() / 2.0, badge.height() / 2.0);
				painter->setPen(text);
				painter->drawText(badge, Qt::AlignCenter, count);
			}
			else
			{
				painter->setPen(dim);
				painter->drawText(badge, Qt::AlignRight | Qt::AlignVCenter, count);
			}

			QFont font(option.font);
			font.setBold(header);
			painter->setFont(font);
			painter->setPen(text);
			const QRect label(area.left(), area.top(), badge.left() - 8 - area.left(), area.height());
			painter->drawText(label, Qt::AlignLeft | Qt::AlignVCenter,
				QFontMetrics(font).elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, label.width()));

			if (header)
			{
				painter->setPen(withAlpha(text, 40));
				painter->drawLine(option.rect.left() + 4, option.rect.bottom(), option.rect.right() - 4, option.rect.bottom());
			}
		}

		void paintLeaf(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index,
			const QRect &area, bool match, bool hover, const QColor &text, const QColor &dim) const
		{
			const QFontMetrics metrics(option.font);
			const int titleTop = match ? area.top() + 5 : area.center().y() - metrics.height() / 2;
			const int titleMid = titleTop + metrics.height() / 2;
			int right = area.right();

			const QRect star = starRect(option.rect.adjusted(2, 1, -2, -1));
			const bool favorite = index.data(kFavoriteRole).toBool();
			if (favorite || hover)
			{
				paintStar(painter, QPointF(star.center().x() + 0.5, titleMid + 0.5), favorite, dim);
			}
			right = star.left() - 2;

			const QString families = index.data(kFamiliesRole).toString();
			painter->setPen(Qt::NoPen);
			for (int f = families.size() - 1; f >= 0; --f)
			{
				painter->setBrush(familyColour(families.at(f).digitValue()).lighter(115));
				painter->drawEllipse(QPointF(right - kDotRadius, titleMid + 0.5), kDotRadius, kDotRadius);
				right -= kDotStep;
			}
			if (!families.isEmpty())
			{
				right -= 6;
			}

			paintTitle(painter, QRect(area.left(), titleTop, right - area.left(), metrics.height()),
				index.data(Qt::DisplayRole).toString(), option.font, text, dim);

			if (match)
			{
				const QFont small = smallFont(option.font);
				painter->setFont(small);
				painter->setPen(dim);
				const QRect crumb(area.left(), titleTop + metrics.height() + 1, right - area.left(), QFontMetrics(small).height());
				painter->drawText(crumb, Qt::AlignLeft | Qt::AlignVCenter,
					QFontMetrics(small).elidedText(index.data(kCrumbRole).toString(), Qt::ElideRight, crumb.width()));
			}
		}

		// "Lead -- rest" draws the lead bold and the rest dim; filter words sit on a highlight.
		void paintTitle(QPainter *painter, const QRect &rect, const QString &label, const QFont &font,
			const QColor &text, const QColor &dim) const
		{
			QString shown = label;
			int leadLength = 0;
			const int split = label.indexOf(" -- ");
			if (split > 0)
			{
				shown = label.left(split) + " " + label.mid(split + 4);
				leadLength = split;
			}
			QVector<bool> marked(shown.size(), false);
			for (int w = 0; w < m_words.size(); ++w)
			{
				const QString &word = m_words.at(w);
				int at = 0;
				while ((at = shown.indexOf(word, at, Qt::CaseInsensitive)) >= 0)
				{
					for (int k = at; k < at + word.length(); ++k)
					{
						marked[k] = true;
					}
					at += word.length();
				}
			}

			QFont bold(font);
			bold.setBold(true);
			int x = rect.left();
			int i = 0;
			while (i < shown.size())
			{
				const bool lead = i < leadLength;
				const bool mark = marked.at(i);
				int j = i + 1;
				while (j < shown.size() && (j < leadLength) == lead && marked.at(j) == mark)
				{
					++j;
				}
				const QFont &pieceFont = lead ? bold : font;
				const QFontMetrics metrics(pieceFont);
				QString piece = shown.mid(i, j - i);
				int width = metrics.horizontalAdvance(piece);
				const bool clipped = x + width > rect.right() + 1;
				if (clipped)
				{
					piece = metrics.elidedText(piece, Qt::ElideRight, rect.right() + 1 - x);
					width = metrics.horizontalAdvance(piece);
				}
				const QRect box(x, rect.top(), width, rect.height());
				if (mark)
				{
					painter->fillRect(box, QColor(215, 160, 60));
					painter->setPen(Qt::black);
				}
				else
				{
					painter->setPen((leadLength > 0 && !lead) ? dim : text);
				}
				painter->setFont(pieceFont);
				painter->drawText(box, Qt::AlignLeft | Qt::AlignVCenter, piece);
				if (clipped)
				{
					break;
				}
				x += width;
				i = j;
			}
		}

		void paintStar(QPainter *painter, const QPointF &centre, bool filled, const QColor &dim) const
		{
			QPolygonF star;
			for (int p = 0; p < 10; ++p)
			{
				const qreal radius = (p % 2 == 0) ? 6.5 : 2.8;
				const qreal angle = -M_PI / 2 + p * M_PI / 5;
				star << QPointF(centre.x() + radius * qCos(angle), centre.y() + radius * qSin(angle));
			}
			if (filled)
			{
				painter->setPen(Qt::NoPen);
				painter->setBrush(QColor(215, 160, 60));
			}
			else
			{
				painter->setPen(QPen(dim, 1.2));
				painter->setBrush(Qt::NoBrush);
			}
			painter->drawPolygon(star);
		}

		QStringList m_words;
	};
}

WBQtCondActDialog::WBQtCondActDialog(void *item, bool isAction, QWidget *parent)
	: QDialog(parent),
	m_ui(new Ui::WBQtCondActDialog),
	m_item(item),
	m_isAction(isAction ? 1 : 0),
	m_updating(false),
	m_flow(NULL),
	m_scope(NULL)
{
	m_ui->setupUi(this);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setWindowTitle(isAction ? "Edit Action" : "Edit Condition");
	m_ui->searchEdit->setPlaceholderText(isAction ? "Filter actions..." : "Filter conditions...");

	const int count = WBQtCondActData_GetTemplateCount(m_isAction);
	for (int i = 0; i < count; i++)
	{
		m_names.append(templateName(m_isAction, i));
		m_names2.append(templateName2(m_isAction, i));
		m_pathIndex.insert(m_names.last(), i);
		int families[16];
		const int numFamilies = WBQtCondActData_GetTemplateFamilies(m_isAction, i, families, 16);
		QString digits;
		for (int f = 0; f < numFamilies; ++f)
		{
			digits += QChar('0' + families[f]);
		}
		m_families.append(digits);
	}

	m_flow = new FlowLayout(m_ui->sentenceHost, 5);
	QFont sentenceFont = m_ui->sentenceHost->font();
	sentenceFont.setPointSizeF(sentenceFont.pointSizeF() + 1.0);
	m_ui->sentenceHost->setFont(sentenceFont);

	m_ui->searchEdit->installEventFilter(this);
	QTreeWidget *tree = m_ui->tree;
	tree->setItemDelegate(new RowDelegate(tree));
	tree->setRootIsDecorated(false);
	tree->setIndentation(16);
	tree->setExpandsOnDoubleClick(false);
	tree->setMouseTracking(true);
	tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	tree->setStyleSheet("QTreeView { show-decoration-selected: 0; }"
		"QTreeView::branch { image: none; border-image: none; background: transparent; }");
	tree->viewport()->setAttribute(Qt::WA_Hover, true);
	tree->viewport()->installEventFilter(this);

	m_scope = new QButtonGroup(this);
	m_scope->addButton(m_ui->scopeAll, kScopeAll);
	m_scope->addButton(m_ui->scopeFavorites, kScopeFavorites);
	m_scope->addButton(m_ui->scopeRecent, kScopeRecent);
	m_ui->scopeBar->setStyleSheet(
		"QToolButton { border: 1px solid palette(mid); border-radius: 10px; padding: 2px 10px; }"
		"QToolButton:hover { background-color: rgba(128, 128, 128, 40); }"
		"QToolButton:checked { background-color: palette(highlight); color: palette(highlighted-text); border-color: palette(highlight); }");
	m_ui->matchLabel->setEnabled(false);
	updateScopeLabels();

	// The key to the row dots, listing Other only when some template has such a parameter.
	QString key;
	const int firstFamily = m_families.join(QString()).contains('0') ? WBQT_PARAM_OTHER : WBQT_PARAM_THING;
	for (int family = firstFamily; family <= WBQT_PARAM_LOGIC; ++family)
	{
		key += QString("<span style=\"color:%1\">&#9679;</span>&nbsp;%2&nbsp;&nbsp; ")
			.arg(familyColour(family).lighter(115).name()).arg(familyName(family));
	}
	QLabel *legend = new QLabel(key, m_ui->scopeBar);
	legend->setToolTip(QString("The dots on each %1 show the parameters it takes, in order.")
		.arg(m_isAction ? "action" : "condition"));
	m_ui->scopeLay->insertWidget(m_ui->scopeLay->indexOf(m_ui->matchLabel), legend);
	m_ui->scopeLay->insertSpacing(m_ui->scopeLay->indexOf(m_ui->matchLabel), 12);
	m_ui->split->setStretchFactor(0, 3);
	m_ui->split->setStretchFactor(1, 2);

	connect(m_ui->tree, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
			this, SLOT(onCurrentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)));
	connect(m_ui->tree, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(onTreeContextMenu(QPoint)));
	connect(m_ui->tree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), this, SLOT(onItemDoubleClicked(QTreeWidgetItem*,int)));
	connect(m_scope, SIGNAL(buttonClicked(int)), this, SLOT(onScopeClicked(int)));
	connect(m_ui->searchEdit, SIGNAL(textChanged(QString)), this, SLOT(onFilterChanged(QString)));
	connect(m_ui->notesToggle, SIGNAL(toggled(bool)), this, SLOT(onNotesToggled(bool)));
	connect(m_ui->buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
	connect(m_ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

	m_ui->notesToggle->setChecked(WBQtCondAct_GetNotesOpen() != 0);
	onNotesToggled(m_ui->notesToggle->isChecked());
	applyTreeFont();

	buildTree(QString());
	renderSentence();
	showHelpForType(WBQtCondActData_GetType(m_item, m_isAction));
	m_ui->tree->setFocus();

	resize(960, 600);
}

WBQtCondActDialog::~WBQtCondActDialog()
{
	delete m_ui;
}

void WBQtCondActDialog::accept()
{
	const int type = WBQtCondActData_GetType(m_item, m_isAction);
	if (type >= 0 && type < WBQtCondActData_GetTemplateCount(m_isAction))
	{
		const QString path = templateName(m_isAction, type);
		QStringList recent = savedList(false);
		recent.removeAll(path);
		recent.prepend(path);
		while (recent.size() > kRecentMax)
		{
			recent.removeLast();
		}
		setSavedList(false, recent);
	}
	QDialog::accept();
}

bool WBQtCondActDialog::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == m_ui->searchEdit && event->type() == QEvent::KeyPress)
	{
		QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
		// Enter takes the first match instead of closing the dialog; with no filter it does nothing.
		if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
		{
			if (!m_ui->searchEdit->text().trimmed().isEmpty())
			{
				selectFirstMatch();
			}
			return true;
		}
		if (keyEvent->key() == Qt::Key_Down)
		{
			m_ui->tree->setFocus();
			return true;
		}
	}
	const bool press = event->type() == QEvent::MouseButtonPress;
	if (watched == m_ui->tree->viewport() && (press || event->type() == QEvent::MouseButtonDblClick))
	{
		QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
		QTreeWidgetItem *item = m_ui->tree->itemAt(mouseEvent->pos());
		if (item != NULL && mouseEvent->button() == Qt::LeftButton)
		{
			const int kind = item->data(0, kKindRole).toInt();
			const QRect rect = m_ui->tree->visualItemRect(item);
			// The star toggles the favorite without picking the row.
			if ((kind == kRowLeaf || kind == kRowMatch) && starRect(rect.adjusted(2, 1, -2, -1)).contains(mouseEvent->pos()))
			{
				if (press)
				{
					toggleFavorite(item->data(0, kTemplateRole).toInt());
				}
				return true;
			}
			// Folders open on a single click; the double click's second press is swallowed.
			if (kind == kRowHeader || kind == kRowFolder)
			{
				if (press)
				{
					item->setExpanded(!item->isExpanded());
				}
				return !press;
			}
		}
	}
	return QDialog::eventFilter(watched, event);
}

QStringList WBQtCondActDialog::savedList(bool favorites) const
{
	static char buf[kSavedListCap];
	buf[0] = 0;
	WBQtCondAct_GetSavedList(m_isAction, favorites ? 1 : 0, buf, sizeof(buf));
	return QString::fromLocal8Bit(buf).split('|', QString::SkipEmptyParts);
}

void WBQtCondActDialog::setSavedList(bool favorites, const QStringList &paths)
{
	WBQtCondAct_SetSavedList(m_isAction, favorites ? 1 : 0, paths.join("|").toLocal8Bit().constData());
}

void WBQtCondActDialog::buildTree(const QString &filter)
{
	m_updating = true;
	QTreeWidget *tree = m_ui->tree;
	tree->clear();

	const QStringList words = filter.split(' ', QString::SkipEmptyParts);
	const int curType = WBQtCondActData_GetType(m_item, m_isAction);
	const int scope = m_scope->checkedId();
	QTreeWidgetItem *selLeaf = NULL;
	QList<int> types;
	if (scope == kScopeAll && words.isEmpty())
	{
		selLeaf = buildCatalog(curType);
		m_ui->matchLabel->setText(QString("%1 %2").arg(m_names.size()).arg(m_isAction ? "actions" : "conditions"));
	}
	else
	{
		if (scope == kScopeAll)
		{
			for (int i = 0; i < m_names.size(); i++)
			{
				if (matchesWords(i, words))
				{
					types.append(i);
				}
			}
		}
		else
		{
			const QStringList paths = savedList(scope == kScopeFavorites);
			for (int p = 0; p < paths.size(); ++p)
			{
				const int type = m_pathIndex.value(paths.at(p), -1);
				if (type >= 0 && matchesWords(type, words))
				{
					types.append(type);
				}
			}
		}
		selLeaf = buildFlat(types, scope == kScopeAll, words, curType);
		m_ui->matchLabel->setText(QString("%1 %2").arg(types.size()).arg(types.size() == 1 ? "match" : "matches"));
	}

	static_cast<RowDelegate *>(tree->itemDelegate())->setWords(words);
	tree->viewport()->update();
	m_updating = false;
	selectCurrentType(selLeaf);
}

// The category tree from the templates' '/'-separated paths; name2 adds a second leaf.
QTreeWidgetItem *WBQtCondActDialog::buildCatalog(int curType)
{
	QTreeWidget *tree = m_ui->tree;
	const QStringList favorites = savedList(true);
	QHash<QString, QTreeWidgetItem *> folders;
	QTreeWidgetItem *selLeaf = NULL;
	for (int i = 0; i < m_names.size(); i++)
	{
		for (int pass = 0; pass < 2; pass++)
		{
			const QString path = (pass == 0) ? m_names.at(i) : m_names2.at(i);
			if (path.isEmpty())
			{
				continue;
			}
			QStringList parts = path.split('/');
			const QString leafLabel = parts.takeLast();
			QTreeWidgetItem *parent = NULL;
			QString key;
			for (int p = 0; p < parts.size(); p++)
			{
				key += parts[p];
				key += '/';
				QTreeWidgetItem *folder = folders.value(key, NULL);
				if (folder == NULL)
				{
					folder = new CatalogItem(parent == NULL ? kRowHeader : kRowFolder);
					folder->setText(0, parts[p]);
					folder->setData(0, kTemplateRole, -1);
					if (parent == NULL)
					{
						tree->addTopLevelItem(folder);
					}
					else
					{
						parent->addChild(folder);
					}
					folders.insert(key, folder);
				}
				parent = folder;
			}
			QTreeWidgetItem *leaf = makeLeaf(parent, leafLabel, i, favorites.contains(m_names.at(i)));
			if (parent == NULL)
			{
				tree->addTopLevelItem(leaf);
			}
			if (pass == 0 && i == curType)
			{
				selLeaf = leaf;
			}
		}
	}
	tree->sortItems(0, Qt::AscendingOrder);

	// Each folder's badge counts the leaves under it.
	QHash<QString, QTreeWidgetItem *>::const_iterator it;
	for (it = folders.constBegin(); it != folders.constEnd(); ++it)
	{
		int leaves = 0;
		QList<QTreeWidgetItem *> pending;
		pending.append(it.value());
		while (!pending.isEmpty())
		{
			QTreeWidgetItem *node = pending.takeLast();
			for (int c = 0; c < node->childCount(); ++c)
			{
				QTreeWidgetItem *child = node->child(c);
				if (child->data(0, kTemplateRole).toInt() >= 0)
				{
					++leaves;
				}
				else
				{
					pending.append(child);
				}
			}
		}
		it.value()->setData(0, kCountRole, leaves);
	}
	return selLeaf;
}

// A flat list, each row with its category path; filter results rank leaf-label hits first.
QTreeWidgetItem *WBQtCondActDialog::buildFlat(const QList<int> &types, bool rank, const QStringList &words, int curType)
{
	QTreeWidget *tree = m_ui->tree;
	if (types.isEmpty())
	{
		QTreeWidgetItem *empty = new CatalogItem(kRowEmpty);
		empty->setFlags(Qt::NoItemFlags);
		empty->setData(0, kTemplateRole, -1);
		const QString noun = m_isAction ? "action" : "condition";
		if (!words.isEmpty())
		{
			empty->setText(0, "No matches");
		}
		else if (m_scope->checkedId() == kScopeFavorites)
		{
			empty->setText(0, QString("Click the star on an %1 to keep it here").arg(noun));
		}
		else
		{
			empty->setText(0, QString("The %1s you pick show up here").arg(noun));
		}
		tree->addTopLevelItem(empty);
		return NULL;
	}

	QList<RankedType> ordered;
	for (int t = 0; t < types.size(); ++t)
	{
		RankedType ranked;
		ranked.type = types.at(t);
		ranked.key = m_names.at(ranked.type).section('/', -1);
		ranked.score = 0;
		if (rank)
		{
			for (int w = 0; w < words.size(); ++w)
			{
				const int at = ranked.key.indexOf(words.at(w), 0, Qt::CaseInsensitive);
				if (at >= 0)
				{
					ranked.score += (at == 0) ? 3 : 2;
				}
			}
		}
		ordered.append(ranked);
	}
	if (rank)
	{
		qStableSort(ordered.begin(), ordered.end());
	}

	const QStringList favorites = savedList(true);
	const QString separator = QString(" ") + QChar(0x203A) + " ";
	QTreeWidgetItem *selLeaf = NULL;
	for (int o = 0; o < ordered.size(); ++o)
	{
		const int type = ordered.at(o).type;
		QTreeWidgetItem *leaf = makeLeaf(NULL, ordered.at(o).key, type, favorites.contains(m_names.at(type)));
		leaf->setData(0, kKindRole, kRowMatch);
		leaf->setData(0, kCrumbRole, m_names.at(type).section('/', 0, -2).replace("/", separator));
		tree->addTopLevelItem(leaf);
		if (type == curType)
		{
			selLeaf = leaf;
		}
	}
	return selLeaf;
}

// Every word must appear somewhere in the template's paths, in any order.
bool WBQtCondActDialog::matchesWords(int type, const QStringList &words) const
{
	for (int w = 0; w < words.size(); ++w)
	{
		if (!m_names.at(type).contains(words.at(w), Qt::CaseInsensitive)
			&& !m_names2.at(type).contains(words.at(w), Qt::CaseInsensitive))
		{
			return false;
		}
	}
	return true;
}

QTreeWidgetItem *WBQtCondActDialog::makeLeaf(QTreeWidgetItem *parent, const QString &label, int type, bool favorite)
{
	QTreeWidgetItem *leaf = new CatalogItem(kRowLeaf);
	leaf->setText(0, label);
	leaf->setData(0, kTemplateRole, type);
	leaf->setData(0, kFamiliesRole, m_families.at(type));
	leaf->setData(0, kFavoriteRole, favorite);

	QString tip = QString(m_names.at(type)).replace("/", QString(" ") + QChar(0x203A) + " ");
	const QString &families = m_families.at(type);
	if (!families.isEmpty())
	{
		QStringList needs;
		for (int f = 0; f < families.size(); ++f)
		{
			needs.append(familyName(families.at(f).digitValue()));
		}
		tip += "\nNeeds: " + needs.join(", ");
	}
	leaf->setToolTip(0, tip);
	if (parent != NULL)
	{
		parent->addChild(leaf);
	}
	return leaf;
}

void WBQtCondActDialog::toggleFavorite(int type)
{
	if (type < 0 || type >= m_names.size())
	{
		return;
	}
	QStringList favorites = savedList(true);
	const bool favorite = !favorites.contains(m_names.at(type));
	if (favorite)
	{
		favorites.append(m_names.at(type));
	}
	else
	{
		favorites.removeAll(m_names.at(type));
	}
	setSavedList(true, favorites);
	updateScopeLabels();
	if (m_scope->checkedId() == kScopeFavorites)
	{
		buildTree(m_ui->searchEdit->text().trimmed());
		return;
	}
	for (QTreeWidgetItemIterator it(m_ui->tree); *it; ++it)
	{
		if ((*it)->data(0, kTemplateRole).toInt() == type)
		{
			(*it)->setData(0, kFavoriteRole, favorite);
		}
	}
}

void WBQtCondActDialog::updateScopeLabels()
{
	int counts[2] = { 0, 0 };
	for (int list = 0; list < 2; ++list)
	{
		const QStringList paths = savedList(list == 0);
		for (int p = 0; p < paths.size(); ++p)
		{
			if (m_pathIndex.contains(paths.at(p)))
			{
				++counts[list];
			}
		}
	}
	m_ui->scopeFavorites->setText(QString(QChar(0x2605)) + QString(" Favorites  %1").arg(counts[0]));
	m_ui->scopeRecent->setText(QString("Recent  %1").arg(counts[1]));
}

void WBQtCondActDialog::onScopeClicked(int scope)
{
	Q_UNUSED(scope);
	buildTree(m_ui->searchEdit->text().trimmed());
	m_ui->tree->setFocus();
}

void WBQtCondActDialog::onItemDoubleClicked(QTreeWidgetItem *item, int column)
{
	Q_UNUSED(column);
	if (item != NULL && item->data(0, kTemplateRole).toInt() >= 0)
	{
		accept();
	}
}

void WBQtCondActDialog::selectCurrentType(QTreeWidgetItem *leaf)
{
	if (leaf != NULL)
	{
		m_updating = true;
		m_ui->tree->setCurrentItem(leaf);
		m_ui->tree->scrollToItem(leaf, QAbstractItemView::PositionAtTop);
		m_updating = false;
	}
}

void WBQtCondActDialog::selectFirstMatch()
{
	for (QTreeWidgetItemIterator it(m_ui->tree); *it; ++it)
	{
		if ((*it)->data(0, kTemplateRole).toInt() >= 0)
		{
			m_ui->tree->setCurrentItem(*it);
			m_ui->tree->scrollToItem(*it);
			m_ui->tree->setFocus();
			return;
		}
	}
}

void WBQtCondActDialog::onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
	Q_UNUSED(previous);
	if (m_updating || current == NULL)
	{
		return;
	}
	const int type = current->data(0, kTemplateRole).toInt();
	if (type < 0)
	{
		return;
	}
	// == the TVN_SELCHANGED handler: only react when the type actually changes.
	if (type != WBQtCondActData_GetType(m_item, m_isAction))
	{
		WBQtCondActData_SetType(m_item, m_isAction, type);
		renderSentence();
		showHelpForType(type);
	}
}

void WBQtCondActDialog::onTreeContextMenu(const QPoint &pos)
{
	QTreeWidgetItem *item = m_ui->tree->itemAt(pos);
	if (item == NULL)
	{
		return;
	}
	const int type = item->data(0, kTemplateRole).toInt();
	if (type < 0)
	{
		return;
	}
	const bool isFavorite = item->data(0, kFavoriteRole).toBool();

	QMenu menu(this);
	QAction *toggle = menu.addAction(isFavorite ? "Remove from Favorites" : "Add to Favorites");
	if (menu.exec(m_ui->tree->viewport()->mapToGlobal(pos)) == toggle)
	{
		toggleFavorite(type);
	}
}

void WBQtCondActDialog::renderSentence()
{
	// Drop the previous sentence; later, since a chip's own click lands here.
	QLayoutItem *old;
	while ((old = m_flow->takeAt(0)) != NULL)
	{
		old->widget()->hide();
		old->widget()->deleteLater();
		delete old;
	}

	// The sentence interleaves uiStrings[0], param[0], uiStrings[1], param[1], ...
	const int numStrings = WBQtCondActData_GetUiStringCount(m_item, m_isAction);
	const int numParams = WBQtCondActData_GetParameterCount(m_item, m_isAction);
	const int total = (numStrings > numParams) ? numStrings : numParams;
	char buf[kTextCap];
	for (int i = 0; i < total; i++)
	{
		if (i < numStrings)
		{
			buf[0] = 0;
			WBQtCondActData_GetUiString(m_item, m_isAction, i, buf, sizeof(buf));
			const QStringList words = QString::fromLocal8Bit(buf).split(' ', QString::SkipEmptyParts);
			for (int w = 0; w < words.size(); ++w)
			{
				m_flow->addWidget(new QLabel(words.at(w), m_ui->sentenceHost));
			}
		}
		if (i < numParams)
		{
			buf[0] = 0;
			WBQtCondActData_GetParameterText(m_item, m_isAction, i, buf, sizeof(buf));
			QString text = QString::fromLocal8Bit(buf);
			if (text.isEmpty())
			{
				text = "???";
			}
			char warnBuf[kBigCap];
			warnBuf[0] = 0;
			WBQtCondActData_GetParameterWarning(m_item, m_isAction, i, warnBuf, sizeof(warnBuf));
			const QString warning = QString::fromLocal8Bit(warnBuf);

			const QColor colour = warning.isEmpty()
				? familyColour(WBQtCondActData_GetParameterFamily(m_item, m_isAction, i))
				: QColor(175, 55, 55);
			QToolButton *chip = new QToolButton(m_ui->sentenceHost);
			chip->setText(warning.isEmpty() ? text : text + "  !");
			chip->setCursor(Qt::PointingHandCursor);
			chip->setToolTip(warning.isEmpty() ? "Click to change" : warning);
			chip->setProperty("paramIndex", i);
			chip->setStyleSheet(QString(
				"QToolButton { background-color: %1; color: white; border: none; border-radius: 4px; padding: 2px 8px; }"
				"QToolButton:hover { background-color: %2; }")
				.arg(colour.name()).arg(colour.lighter(125).name()));
			connect(chip, SIGNAL(clicked()), this, SLOT(onChipClicked()));
			m_flow->addWidget(chip);
		}
	}
	updateWarnings();
}

void WBQtCondActDialog::onChipClicked()
{
	bool ok = false;
	const int index = sender()->property("paramIndex").toInt(&ok);
	if (!ok)
	{
		return;
	}
	// Pops the (still MFC) parameter editor; the sentence and warnings re-render on return.
	WBQtCondAct_EditParameter(m_item, m_isAction, index);
	renderSentence();
}

void WBQtCondActDialog::updateWarnings()
{
	char warnBuf[kBigCap];
	char infoBuf[kBigCap];
	warnBuf[0] = 0;
	infoBuf[0] = 0;
	WBQtCondActData_GetWarnings(m_item, m_isAction, warnBuf, sizeof(warnBuf), infoBuf, sizeof(infoBuf));
	const QString warnings = QString::fromLocal8Bit(warnBuf).trimmed();
	const QString information = QString::fromLocal8Bit(infoBuf).trimmed();

	QLabel *label = m_ui->warningsLabel;
	if (warnings.isEmpty() && information.isEmpty())
	{
		label->hide();
		return;
	}
	const bool warn = !warnings.isEmpty();
	const QColor accent = warn ? QColor(200, 70, 70) : QColor(80, 140, 210);
	label->setStyleSheet(QString("QLabel { border-left: 4px solid %1; background-color: rgba(%2, %3, %4, 40); padding: 6px 8px; }")
		.arg(accent.name()).arg(accent.red()).arg(accent.green()).arg(accent.blue()));
	label->setText(QString("<b>%1</b><br>%2")
		.arg(warn ? "Warnings" : "Information")
		.arg((warn ? warnings : information).toHtmlEscaped().replace("\n", "<br>")));
	label->show();
}

void WBQtCondActDialog::showHelpForType(int type)
{
	const bool valid = (type >= 0 && type < WBQtCondActData_GetTemplateCount(m_isAction));
	m_ui->helpLabel->setText(valid ? templateHelp(m_isAction, type) : QString());
}

void WBQtCondActDialog::onNotesToggled(bool open)
{
	m_ui->notesToggle->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
	m_ui->helpLabel->setVisible(open);
	WBQtCondAct_SetNotesOpen(open ? 1 : 0);
}

void WBQtCondActDialog::onFilterChanged(const QString &text)
{
	buildTree(text.trimmed());
}

void WBQtCondActDialog::applyTreeFont()
{
	// The script window's Compress Script setting: 14px compressed, 16px not.
	QFont font = m_ui->tree->font();
	font.setPixelSize(WBQtCondAct_GetCompress() ? 14 : 16);
	m_ui->tree->setFont(font);
}

// ===================== the modal entry point =====================

extern "C" int WBQtCondAct_Run(void *item, int isAction)
{
	if (item == NULL)
	{
		return 0;
	}
	// == EditCondition/EditAction::OnInitDialog clearing the item's warning flag.
	WBQtCondActData_ClearWarningFlag(item, isAction);
	// Parent to the active Qt modal (the script-edit dialog); exec() is application-modal, and
	// the MFC frame is already disabled by the outer WBQtScriptEdit_Run.
	if (WBQtScript_GetNewDesign() == 0)
	{
		WBQtCondActDialogClassic classic(item, isAction != 0, QApplication::activeModalWidget());
		return (classic.exec() == QDialog::Accepted) ? 1 : 0;
	}
	WBQtCondActDialog dlg(item, isAction != 0, QApplication::activeModalWidget());
	const int rc = dlg.exec();
	return (rc == QDialog::Accepted) ? 1 : 0;
}
