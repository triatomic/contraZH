// WBQtNewBrushDialog.h -- defines a WorldBuilder-only terrain brush from the Terrain Material
// panel. A brush paints an existing terrain under a new name, or registers a new texture; it is
// saved to WorldBuilderQt\TerrainBrushes.ini through WBQtTerrainMaterial_CreateBrush.
#ifndef WB_QT_NEW_BRUSH_DIALOG_H
#define WB_QT_NEW_BRUSH_DIALOG_H

#include <QDialog>
#include <QIcon>
#include <QImage>
#include <QStringList>

namespace Ui { class WBQtNewBrushDialog; }	// generated from WBQtNewBrushDialog.ui

class WBQtNewBrushDialog : public QDialog
{
	Q_OBJECT
public:
	WBQtNewBrushDialog(const QStringList &categories, const QString &category, const QString &terrain, QWidget *parent);
	virtual ~WBQtNewBrushDialog();

	// The texture class the new brush paints, valid after the dialog is accepted.
	int createdTexClass() const { return m_texClass; }
	QString createdName() const { return m_name; }

public slots:
	virtual void accept();

private slots:
	void onModeChanged();
	void onTerrainChanged();
	void onBrowse();
	void updatePreview();
	void updateKeyEnables();
	void onSaveKeys();
	void onTexturesChanged();

private:
	struct Keys
	{
		bool blendEdges;
		bool restrictConstruction;
		bool hasGlintStrength;
		double glintStrength;
		bool hasGlintGloss;
		double glintGloss;
	};

	static QIcon swatchIcon(int texClass);
	Keys readKeys() const;
	void showKeys(const Keys &keys);
	void showTerrainKeys();
	int changedTerrainKeys() const;
	bool saveTerrainKeys();
	QStringList textureList() const;
	static QString nameFromTexture(const QString &texture);
	int createBrush(const QString &name, const QString &texture, QString &error);

	Ui::WBQtNewBrushDialog *m_ui;	// owns the static widget tree (WBQtNewBrushDialog.ui)
	int m_texClass;
	QString m_name;
	bool m_textureMode;
	Keys m_textureKeys;	// what the user set for a new texture, kept while an existing terrain shows its own
	Keys m_terrainKeys;	// the selected terrain's keys as last loaded or saved
};

#endif // WB_QT_NEW_BRUSH_DIALOG_H
