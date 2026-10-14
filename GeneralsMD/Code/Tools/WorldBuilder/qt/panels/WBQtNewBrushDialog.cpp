// WBQtNewBrushDialog.cpp -- see WBQtNewBrushDialog.h.
#include "WBQtNewBrushDialog.h"
#include "ui_WBQtNewBrushDialog.h"
#include "WBQtComboStyle.h"
#include "WBQtTerrainMaterialBridge.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QMessageBox>
#include <QPixmap>
#include <QRegExp>

WBQtNewBrushDialog::WBQtNewBrushDialog(const QStringList &categories, const QString &category, const QString &terrain, QWidget *parent)
	: QDialog(parent),
	  m_ui(new Ui::WBQtNewBrushDialog),
	  m_texClass(-1)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	m_ui->setupUi(this);
	WBQtComboStyle::applyPopupScrollRecursive(this);

	m_ui->categoryCombo->addItems(categories);
	m_ui->categoryCombo->setEditText(category);

	const int cap = 512;
	char nameBuf[cap];
	QStringList terrains;
	QHash<QString, int> texClasses;
	int count = WBQtTerrainMaterial_GetTexClassCount();
	for (int i = 0; i < count; ++i)
	{
		if (WBQtTerrainMaterial_GetTexClassTerrainName(i, nameBuf, cap))
		{
			QString name = QString::fromLatin1(nameBuf);
			terrains.append(name);
			texClasses.insert(name, i);
		}
	}
	terrains.sort(Qt::CaseInsensitive);
	for (int i = 0; i < terrains.size(); ++i)
	{
		int texClass = texClasses.value(terrains.at(i));
		m_ui->terrainCombo->addItem(swatchIcon(texClass), terrains.at(i), texClass);
	}
	int terrainIndex = m_ui->terrainCombo->findText(terrain);
	if (terrainIndex >= 0)
	{
		m_ui->terrainCombo->setCurrentIndex(terrainIndex);
	}

	connect(m_ui->useTerrain, SIGNAL(toggled(bool)), this, SLOT(onModeChanged()));
	connect(m_ui->useTexture, SIGNAL(toggled(bool)), this, SLOT(onModeChanged()));
	connect(m_ui->terrainCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updatePreview()));
	connect(m_ui->textureEdit, SIGNAL(textChanged(QString)), this, SLOT(updatePreview()));
	connect(m_ui->browseBtn, SIGNAL(clicked()), this, SLOT(onBrowse()));
	connect(m_ui->okButton, SIGNAL(clicked()), this, SLOT(accept()));
	connect(m_ui->cancelButton, SIGNAL(clicked()), this, SLOT(reject()));
	onModeChanged();

	resize(700, 400);
}

// The terrain's first tile, as the Terrain Material swatches show it.
QIcon WBQtNewBrushDialog::swatchIcon(int texClass)
{
	int extent = WBQtTerrainMaterial_GetSwatchExtent();
	QByteArray bgra(extent * extent * 4, 0);
	if (extent <= 0 || !WBQtTerrainMaterial_GetSwatchPixels(texClass, reinterpret_cast<unsigned char*>(bgra.data()), bgra.size()))
	{
		return QIcon();
	}
	return QIcon(QPixmap::fromImage(bottomUpBgraToImage(bgra, extent)));
}

QImage WBQtNewBrushDialog::bottomUpBgraToImage(const QByteArray &bgra, int extent)
{
	QImage img(extent, extent, QImage::Format_RGB888);
	for (int y = 0; y < extent; ++y)
	{
		const unsigned char *src = reinterpret_cast<const unsigned char*>(bgra.constData()) + (extent - 1 - y) * extent * 4;
		unsigned char *dst = img.scanLine(y);
		for (int x = 0; x < extent; ++x)
		{
			dst[x * 3 + 0] = src[x * 4 + 2];
			dst[x * 3 + 1] = src[x * 4 + 1];
			dst[x * 3 + 2] = src[x * 4 + 0];
		}
	}
	return img;
}

void WBQtNewBrushDialog::updatePreview()
{
	int maxExtent = WBQtTerrainMaterial_GetPreviewMaxExtent();
	QByteArray bgra(maxExtent * maxExtent * 4, 0);
	unsigned char *pixels = reinterpret_cast<unsigned char*>(bgra.data());
	int extent = 0;
	if (m_ui->useTexture->isChecked())
	{
		QByteArray texture = m_ui->textureEdit->text().trimmed().toLatin1();
		extent = WBQtTerrainMaterial_GetTexturePreview(texture.constData(), pixels, bgra.size());
	}
	else if (m_ui->terrainCombo->currentIndex() >= 0)
	{
		int texClass = m_ui->terrainCombo->currentData().toInt();
		extent = WBQtTerrainMaterial_GetTexClassPreview(texClass, pixels, bgra.size());
	}

	if (extent <= 0)
	{
		m_ui->preview->setPixmap(QPixmap());
		m_ui->preview->setText("No preview");
		m_ui->previewInfo->clear();
		return;
	}

	QImage img = bottomUpBgraToImage(bgra, extent);
	QSize box = m_ui->preview->size();
	bool upscale = (box.width() > extent);
	m_ui->preview->setPixmap(QPixmap::fromImage(img).scaled(box, Qt::KeepAspectRatio,
		upscale ? Qt::FastTransformation : Qt::SmoothTransformation));
	int tiles = extent / WBQtTerrainMaterial_GetSwatchExtent();
	m_ui->previewInfo->setText(QString("%1 x %1 tiles, %2 x %2 px").arg(tiles).arg(extent));
}

WBQtNewBrushDialog::~WBQtNewBrushDialog()
{
	delete m_ui;
}

void WBQtNewBrushDialog::onModeChanged()
{
	bool texture = m_ui->useTexture->isChecked();
	m_ui->terrainCombo->setEnabled(!texture);
	m_ui->textureEdit->setEnabled(texture);
	m_ui->browseBtn->setEnabled(texture);
	m_ui->textureNote->setEnabled(texture);
	updatePreview();
}

void WBQtNewBrushDialog::onBrowse()
{
	char dirBuf[256];
	WBQtTerrainMaterial_GetTerrainTextureDir(dirBuf, sizeof(dirBuf));
	QDir texDir(QDir::current().absoluteFilePath(QString::fromLatin1(dirBuf)));
	QString startIn = texDir.exists() ? texDir.absolutePath() : QDir::currentPath();

	QString path = QFileDialog::getOpenFileName(this, "Terrain Texture", startIn,
		"Terrain textures (*.tga);;All files (*.*)");
	if (path.isEmpty())
	{
		return;
	}

	QFileInfo picked(path);
	QString rel = texDir.relativeFilePath(picked.absoluteFilePath());
	if (rel.startsWith("..") || QDir::isAbsolutePath(rel))
	{
		// The game reads terrain textures only from the texture folder, so bring the file in under
		// a name its INI reader keeps whole.
		QString fileName = picked.fileName();
		fileName.replace(QRegExp("[\\s;=\"]"), "_");
		QString dest = texDir.filePath(fileName);
		if (QFile::exists(dest))
		{
			QMessageBox::warning(this, "New Terrain Brush",
				QString("%1 already exists. Rename the file and pick it again.").arg(QDir::toNativeSeparators(dest)));
			return;
		}
		QDir().mkpath(texDir.absolutePath());
		if (!QFile::copy(picked.absoluteFilePath(), dest))
		{
			QMessageBox::warning(this, "New Terrain Brush",
				QString("Could not copy the texture to %1.").arg(QDir::toNativeSeparators(dest)));
			return;
		}
		rel = fileName;
	}
	m_ui->textureEdit->setText(rel);

	if (m_ui->nameEdit->text().trimmed().isEmpty())
	{
		QString name = picked.completeBaseName();
		name.replace(QRegExp("[^A-Za-z0-9_.-]"), "_");
		m_ui->nameEdit->setText(name);
	}
}

void WBQtNewBrushDialog::accept()
{
	bool texture = m_ui->useTexture->isChecked();
	QByteArray name = m_ui->nameEdit->text().trimmed().toLatin1();
	QByteArray category = m_ui->categoryCombo->currentText().trimmed().toLatin1();
	QByteArray terrainName = texture ? QByteArray() : m_ui->terrainCombo->currentText().toLatin1();
	QByteArray textureName = texture ? m_ui->textureEdit->text().trimmed().toLatin1() : QByteArray();

	if (texture && textureName.isEmpty())
	{
		QMessageBox::warning(this, "New Terrain Brush", "Pick a texture file.");
		return;
	}

	char error[512];
	error[0] = 0;
	int texClass = WBQtTerrainMaterial_CreateBrush(name.constData(), category.constData(),
		terrainName.constData(), textureName.constData(), error, sizeof(error));
	if (texClass < 0)
	{
		QMessageBox::warning(this, "New Terrain Brush", QString::fromLatin1(error));
		return;
	}
	if (error[0] != 0)
	{
		QMessageBox::warning(this, "New Terrain Brush", QString("The brush was created. %1").arg(QString::fromLatin1(error)));
	}
	m_texClass = texClass;
	m_name = QString::fromLatin1(name);
	QDialog::accept();
}
