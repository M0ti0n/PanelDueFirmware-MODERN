/*
 * UserInterface.cpp
 *
 *  Created on: 7 Jan 2017
 *      Author: David
 */

#include <UI/UserInterface.hpp>

#include <ctype.h>

#include "Configuration.hpp"
#include "FileManager.hpp"
#include "FlashData.hpp"

#include "Hardware/Buzzer.hpp"
#include "Hardware/Reset.hpp"
#include "Hardware/SerialIo.hpp"
#include "Hardware/SysTick.hpp"

#include "Icons/Icons.hpp"
#include "Library/Misc.hpp"
#include "ObjectModel/BedOrChamber.hpp"
#include "ObjectModel/PrinterStatus.hpp"
#include "PanelDue.hpp"
#include "Version.hpp"

#if DISPLAY_X == 800
extern const uint8_t glcd19x21[];
#endif

#include <General/SafeVsnprintf.h>
#include <General/SimpleMath.h>
#include <General/String.h>
#include <General/StringFunctions.h>

#include <ObjectModel/Axis.hpp>
#include <ObjectModel/Utils.hpp>

#include <UI/MessageLog.hpp>
#include <UI/Popup.hpp>
#include <UI/UserInterfaceConstants.hpp>

MainWindow mgr;

#define DEBUG 0
#include "Debug.hpp"

// Public fields
TextField *fwVersionField, *userCommandField, *ipAddressField;
IntegerField *freeMem;
StaticTextField *touchCalibInstruction, *debugField;
StaticTextField *messageTextFields[numMessageRows], *messageTimeFields[numMessageRows];

static const ColourScheme *colours;

// Private fields
static const size_t machineNameLength = 30;
static const size_t printingFileLength = 40;
static const size_t zprobeBufLength = 12;
static const size_t generatedByTextLength = 50;
static const size_t lastModifiedTextLength = 20;
static const size_t printTimeTextLength = 12;		// e.g. 11h 55m
static const size_t controlPageMacroTextLength = 50;
static const size_t ipAddressLength = 45;	// IPv4 needs max 15 but IPv6 can go up to 45

static String<ipAddressLength> ipAddress;

struct FileListButtons
{
	SingleButton *scrollLeftButton, *scrollRightButton, *folderUpButton;
	IntegerField *errorField;
};

static StaticTextField *fileListPopupNoFiles;

static FileListButtons filesListButtons, macrosListButtons;
static SingleButton *changeCardButton;

static TextButton *filenameButtons[NumDisplayedFiles];
static TextButton *macroButtons[NumDisplayedMacros];
static TextButton *controlPageMacroButtons[NumControlPageMacroButtons];
static String<controlPageMacroTextLength> controlPageMacroText[NumControlPageMacroButtons];

static PopupWindow *setTempPopup, *setRPMPopup, *movePopup, *extrudePopup, *fileListPopup, *macrosPopup, *fileDetailPopup, *baudPopup,
		*volumePopup, *infoTimeoutPopup, *screensaverTimeoutPopup, *babystepAmountPopup, *feedrateAmountPopup, *areYouSurePopup, *keyboardPopup, *languagePopup, *coloursPopup, *screensaverPopup, *firmwareUpdatePopup;
static StaticTextField *areYouSureTextField, *areYouSureQueryField;
static DisplayField *emptyRoot, *baseRoot, *commonRoot, *controlRoot, *controlToolsRoot, *controlMovementRoot, *controlExtrusionRoot, *printRoot, *statusJobStatusRoot, *statusTuneRoot, *statusJobRoot, *statusObjectsRoot, *messageRoot, *setupRoot;
static SingleButton *homeAllButton, *bedCompButton;
static IconButtonWithText *homeButtons[MaxDisplayableAxes], *toolButtons[MaxSlots];

// -- START: New fields for subpage grouping and top tabs (CONTROL/STATUS/SYSTEM)
// Top-subtab buttons for CONTROL page
static TextButton *controlTopTools = nullptr;
static TextButton *controlTopMovement = nullptr;
static TextButton *controlTopExtrusion = nullptr;
static TextButton *controlTopMacros = nullptr;

// Arrays to hold pointers to fields belonging to each subpage so we can show/hide them as a group
static DisplayField *controlToolsFields[64];
static size_t controlToolsFieldCount = 0;
static DisplayField *controlMovementFields[64];
static size_t controlMovementFieldCount = 0;
static DisplayField *controlExtrusionFields[64];
static size_t controlExtrusionFieldCount = 0;
static DisplayField *controlMacrosFields[64];
static size_t controlMacrosFieldCount = 0;

// STATUS subpages
static DisplayField *statusJobStatusFields[64];
static size_t statusJobStatusFieldCount = 0;
static DisplayField *statusTuneFields[64];
static size_t statusTuneFieldCount = 0;
static DisplayField *statusJobFields[64];
static size_t statusJobFieldCount = 0;

// SYSTEM subpages
static DisplayField *systemConsoleFields[64];
static size_t systemConsoleFieldCount = 0;
static DisplayField *systemAlertsFields[64];
static size_t systemAlertsFieldCount = 0;
static DisplayField *systemSettingsFields[64];
static size_t systemSettingsFieldCount = 0;

static UiPage currentUiPage = UiPage::ControlTools;

#if DISPLAY_X == 800
// CONTROL > TOOLS modern 800x480 page.
static constexpr unsigned int ControlToolVisibleColumns = 5;
static constexpr unsigned int ControlToolPagedColumns = 4;
static constexpr unsigned int ControlToolMaxHeaters = 32;
static unsigned int controlToolPage = 0;
static int controlToolActiveTarget[ControlToolMaxHeaters] = { 0 };
static int controlToolStandbyTarget[ControlToolMaxHeaters] = { 0 };

enum class ControlToolResourceType : uint8_t { None, Tool, Bed, Chamber };
struct ControlToolResource
{
	ControlToolResourceType type = ControlToolResourceType::None;
	int index = -1;
	int heater = -1;
};
static ControlToolResource controlToolVisibleResource[ControlToolVisibleColumns];
static ModernCard *controlToolHeaderCards[ControlToolVisibleColumns] = { nullptr };
static ModernResourceLabel *controlToolNameFields[ControlToolVisibleColumns] = { nullptr };
static StaticTextField *controlToolCurrentFields[ControlToolVisibleColumns] = { nullptr };
static ModernTemperatureButton *controlToolActiveButtons[ControlToolVisibleColumns] = { nullptr };
static ModernTemperatureButton *controlToolStandbyButtons[ControlToolVisibleColumns] = { nullptr };
static ModernPowerButton *controlToolPowerButtons[ControlToolVisibleColumns] = { nullptr };
static String<16> controlToolNameText[ControlToolVisibleColumns];
static String<20> controlToolCurrentText[ControlToolVisibleColumns];
static String<12> controlToolActiveText[ControlToolVisibleColumns];
static String<12> controlToolStandbyText[ControlToolVisibleColumns];
static ModernIconButton *controlToolPageUpButton = nullptr;
static ModernIconButton *controlToolPageDownButton = nullptr;

static PopupWindow *controlTempNumpadPopup = nullptr;
static StaticTextField *controlTempNumpadValueField = nullptr;
static ModernTextButton *controlTempNumpadResourceField = nullptr;
static String<8> controlTempNumpadValueText;
static String<16> controlTempNumpadResourceText;
static ControlToolResource controlTempNumpadResource;
static bool controlTempNumpadActiveTarget = true;
static unsigned int controlTempNumpadValue = 0;
static bool controlTempNumpadFresh = true;

static PopupWindow *controlToolChangePopup = nullptr;
static ModernTextButton *controlToolChangeFromField = nullptr;
static ModernTextButton *controlToolChangeToField = nullptr;
static String<12> controlToolChangeFromText;
static String<12> controlToolChangeToText;
static int controlToolChangeTarget = NoTool;

// CONTROL > MOVE modern 800x480 page.
static constexpr unsigned int ControlMoveAxisCount = 3;
static constexpr unsigned int ControlMoveStepCount = 5;
static const char * const controlMoveStepText[ControlMoveStepCount] = { "0.1", "0.02", "1", "10", "50" };
static unsigned int controlMoveSelectedStep = 2;       // 1 mm, as shown in the v7 mock-up
static StaticTextField *controlMovePositionFields[ControlMoveAxisCount] = { nullptr };
static String<16> controlMovePositionText[ControlMoveAxisCount];
static bool controlMovePositionValid[ControlMoveAxisCount] = { false, false, false };
static float controlMovePosition[ControlMoveAxisCount] = { 0.0f, 0.0f, 0.0f };
static ModernTextButton *controlMoveStepButtons[ControlMoveStepCount] = { nullptr };
static ModernHomeButton *controlMoveHomeButtons[ControlMoveAxisCount] = { nullptr };
static ModernHomeButton *controlMoveHomeAllButton = nullptr;
static ModernBedCompButton *controlMoveBedCompButton = nullptr;

// CONTROL > EXTRUDE modern 800x480 page. The tool rows are information-only;
// extrusion and retraction always use RRF's current active tool.
static constexpr unsigned int ControlExtrudeToolsPerPage = 4;
static constexpr unsigned int ControlExtrudeSpeedCount = 4;
static constexpr unsigned int ControlExtrudeDistanceCount = 4;
static const char * const controlExtrudeSpeedText[ControlExtrudeSpeedCount] = { "2 mm/s", "5 mm/s", "10 mm/s", "20 mm/s" };
static const unsigned int controlExtrudeSpeedFeedrate[ControlExtrudeSpeedCount] = { 120, 300, 600, 1200 };
static const char * const controlExtrudeDistanceText[ControlExtrudeDistanceCount] = { "10 mm", "20 mm", "50 mm", "150 mm" };
static const char * const controlExtrudeDistanceParam[ControlExtrudeDistanceCount] = { "10", "20", "50", "150" };
static unsigned int controlExtrudeToolPage = 0;
static unsigned int controlExtrudeSelectedSpeed = 1;       // 5 mm/s, as shown in v10
static unsigned int controlExtrudeSelectedDistance = 2;    // 50 mm, as shown in v10
static ModernCard *controlExtrudeToolCards[ControlExtrudeToolsPerPage] = { nullptr };
static StaticTextField *controlExtrudeToolNameFields[ControlExtrudeToolsPerPage] = { nullptr };
static StaticTextField *controlExtrudeToolTempFields[ControlExtrudeToolsPerPage] = { nullptr };
static String<12> controlExtrudeToolNameText[ControlExtrudeToolsPerPage];
static String<20> controlExtrudeToolTempText[ControlExtrudeToolsPerPage];
static ModernTextButton *controlExtrudeSpeedButtons[ControlExtrudeSpeedCount] = { nullptr };
static ModernTextButton *controlExtrudeDistanceButtons[ControlExtrudeDistanceCount] = { nullptr };
static ModernTextButton *controlExtrudeRetractButton = nullptr;
static ModernTextButton *controlExtrudeExtrudeButton = nullptr;
static ModernIconButton *controlExtrudePageUpButton = nullptr;
static ModernIconButton *controlExtrudePageDownButton = nullptr;
static float controlColdExtrudeTemperature = 0.0f;
static float controlColdRetractTemperature = 0.0f;
static bool controlColdExtrudeTemperatureValid = false;
static bool controlColdRetractTemperatureValid = false;

// Reusable modern alert popup.  MOVE is the first consumer, but it is kept
// global so other modern pages can use the same interaction later.
static PopupWindow *modernAlertPopup = nullptr;
static ModernTextButton *modernAlertMessageField = nullptr;
static String<64> modernAlertMessageText;

static constexpr unsigned int StatusObjectsPerPage = 5;
static constexpr unsigned int StatusMaxObjects = 20;       // RRF 3.6 exposes up to 20 build objects on Duet 2
static unsigned int statusObjectPage = 0;
static int selectedStatusObject = -1;
static int currentStatusObject = -1;
static int pendingStatusObjectCancel = -1;
static unsigned int statusObjectCount = 0;
static bool statusObjectsDirty = false;
static bool statusObjectsNeedFullRefresh = false;

struct StatusObjectInfo
{
	String<32> name;
	float xMin = 0.0f, xMax = 0.0f, yMin = 0.0f, yMax = 0.0f;
	bool present = false;
	bool cancelled = false;
	bool xValid = false;
	bool yValid = false;
};

static StatusObjectInfo statusObjects[StatusMaxObjects];
static ModernTextButton *statusObjectNumberButtons[StatusObjectsPerPage] = { nullptr };
static ModernTextButton *statusObjectNameButtons[StatusObjectsPerPage] = { nullptr };
static ModernIconButton *statusObjectPageUpButton = nullptr;
static ModernIconButton *statusObjectPageDownButton = nullptr;
static ModernTextButton *statusObjectMarkers[StatusMaxObjects] = { nullptr };
static String<4> statusObjectRowNumberText[StatusObjectsPerPage];
static String<32> statusObjectRowNameText[StatusObjectsPerPage];
static String<4> statusObjectMarkerText[StatusMaxObjects];
static PopupWindow *statusObjectCancelPopup = nullptr;
static ModernTextButton *statusObjectCancelNumber = nullptr;
static ModernTextButton *statusObjectCancelName = nullptr;
static String<4> statusObjectCancelNumberText;
static String<32> statusObjectCancelNameText;

static float statusObjectAxisMin[MaxTotalAxes] = { 0.0f };
static float statusObjectAxisMax[MaxTotalAxes] = { 0.0f };
static bool statusObjectAxisMinValid[MaxTotalAxes] = { false };
static bool statusObjectAxisMaxValid[MaxTotalAxes] = { false };
static int statusObjectXAxis = -1, statusObjectYAxis = -1;

class StatusObjectMapField : public DisplayField
{
private:
	PixelNumber height;
	PixelNumber canvasX, canvasY, canvasWidth, canvasHeight;
	Colour pageColour, tileColour, mapBorderColour, axisColour;
	float xMin = 0.0f, xMax = 0.0f, yMin = 0.0f, yMax = 0.0f;
	bool boundsValid = false;

	void GetBedRect(PixelNumber& bx, PixelNumber& by, PixelNumber& bw, PixelNumber& bh) const
	{
		bx = canvasX; by = canvasY; bw = canvasWidth; bh = canvasHeight;
		if (!boundsValid)
		{
			return;
		}
		const float xs = xMax - xMin;
		const float ys = yMax - yMin;
		if (xs <= 0.0f || ys <= 0.0f)
		{
			return;
		}
		if (xs * static_cast<float>(canvasHeight) >= ys * static_cast<float>(canvasWidth))
		{
			bh = static_cast<PixelNumber>((static_cast<float>(canvasWidth) * ys / xs) + 0.5f);
			if (bh < 1) bh = 1;
			by = canvasY + (canvasHeight - bh) / 2;
		}
		else
		{
			bw = static_cast<PixelNumber>((static_cast<float>(canvasHeight) * xs / ys) + 0.5f);
			if (bw < 1) bw = 1;
			bx = canvasX + (canvasWidth - bw) / 2;
		}
	}

protected:
	PixelNumber GetHeight() const override { return height; }

public:
	StatusObjectMapField(PixelNumber py, PixelNumber px, PixelNumber pw, PixelNumber ph,
		PixelNumber pcx, PixelNumber pcy, PixelNumber pcw, PixelNumber pch,
		Colour page, Colour tile, Colour borderColour, Colour axes)
		: DisplayField(py, px, pw), height(ph), canvasX(pcx), canvasY(pcy),
		  canvasWidth(pcw), canvasHeight(pch), pageColour(page), tileColour(tile),
		  mapBorderColour(borderColour), axisColour(axes) { }

	void SetBounds(float pxMin, float pxMax, float pyMin, float pyMax, bool valid)
	{
		if (boundsValid != valid || xMin != pxMin || xMax != pxMax || yMin != pyMin || yMax != pyMax)
		{
			boundsValid = valid; xMin = pxMin; xMax = pxMax; yMin = pyMin; yMax = pyMax;
			changed = true;
		}
	}

	void GetBedBounds(PixelNumber& bx, PixelNumber& by, PixelNumber& bw, PixelNumber& bh) const
	{
		GetBedRect(bx, by, bw, bh);
	}

	bool Project(float px, float py, PixelNumber& screenX, PixelNumber& screenY) const
	{
		if (!boundsValid || xMax <= xMin || yMax <= yMin)
		{
			return false;
		}
		PixelNumber bx, by, bw, bh;
		GetBedRect(bx, by, bw, bh);
		float xr = (px - xMin) / (xMax - xMin);
		float yr = (py - yMin) / (yMax - yMin);
		if (xr < 0.0f) xr = 0.0f; else if (xr > 1.0f) xr = 1.0f;
		if (yr < 0.0f) yr = 0.0f; else if (yr > 1.0f) yr = 1.0f;
		screenX = bx + static_cast<PixelNumber>(xr * static_cast<float>(bw - 1));
		screenY = by + bh - 1 - static_cast<PixelNumber>(yr * static_cast<float>(bh - 1));
		return true;
	}

	void Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset) override
	{
		if (!full && !changed) return;
		lcd.setColor(pageColour);
		lcd.fillRect(x + xOffset, y + yOffset, x + xOffset + width - 1, y + yOffset + height - 1);

		PixelNumber bx, by, bw, bh;
		GetBedRect(bx, by, bw, bh);
		bx += xOffset; by += yOffset;
		lcd.setColor(tileColour);
		lcd.fillRect(bx, by, bx + bw - 1, by + bh - 1);
		lcd.setColor(mapBorderColour);
		lcd.drawRect(bx, by, bx + bw - 1, by + bh - 1);
		if (bw > 2 && bh > 2) lcd.drawRect(bx + 1, by + 1, bx + bw - 2, by + bh - 2);

		const int ox = static_cast<int>(bx) - 12;
		const int oy = static_cast<int>(by + bh) + 12;
		const int xt = static_cast<int>(bx + bw) + 12;
		const int yt = static_cast<int>(by) - 6;
		lcd.setColor(axisColour);
		lcd.drawLine(ox, oy, xt, oy);
		lcd.drawLine(ox, oy, ox, yt);
		lcd.drawLine(xt, oy, xt - 8, oy - 5);
		lcd.drawLine(xt, oy, xt - 8, oy + 5);
		lcd.drawLine(ox, yt, ox - 5, yt + 8);
		lcd.drawLine(ox, yt, ox + 5, yt + 8);
		lcd.setTransparentBackground(true);
		lcd.setFont(glcd19x21);
		lcd.setTextPos((ox > 18) ? ox - 18 : 0, oy + 4); lcd.printf("0");
		lcd.setTextPos(xt - 2, oy + 4); lcd.printf("X");
		lcd.setTextPos((ox > 7) ? ox - 7 : 0, (yt > 24) ? yt - 24 : 0); lcd.printf("Y");
		lcd.setTransparentBackground(false);
		changed = false;
	}
};

static StatusObjectMapField *statusObjectMap = nullptr;

static constexpr unsigned int StatusJobRows = 6;
static ModernTextButton *statusJobFileButtons[StatusJobRows] = { nullptr };
static ModernIconButton *statusJobPageUpButton = nullptr;
static ModernIconButton *statusJobPageDownButton = nullptr;
static PopupWindow *statusJobStartPopup = nullptr;
static ModernTextButton *statusJobStartFileField = nullptr;
static bool statusJobCanScrollEarlier = false;
static bool statusJobCanScrollLater = false;
static bool statusJobInSubdir = false;

// STATUS > JOB STATUS. The nine cards are intentionally read-only; tuning belongs on STATUS > TUNE.
enum class JobStatusTileType : uint8_t
{
	ToolTemp, BedTemp, ChamberTemp, FanPart, FanAux, FanCha, SpeedReq, SpeedCur, FlowFactor, FlowVol
};
static constexpr unsigned int JobStatusTileCount = 9;
static constexpr unsigned int JobStatusMaxHeaters = 32;
static JobStatusTileType jobStatusTiles[JobStatusTileCount] =
{
	JobStatusTileType::ToolTemp, JobStatusTileType::BedTemp, JobStatusTileType::ChamberTemp,
	JobStatusTileType::FanPart, JobStatusTileType::FanAux, JobStatusTileType::FanCha,
	JobStatusTileType::SpeedCur, JobStatusTileType::FlowFactor, JobStatusTileType::FlowVol
};
static ModernCard *jobStatusCards[JobStatusTileCount] = { nullptr };
static StaticTextField *jobStatusLabels[JobStatusTileCount] = { nullptr };
static StaticTextField *jobStatusValues[JobStatusTileCount] = { nullptr };
static String<20> jobStatusLabelText[JobStatusTileCount];
static String<24> jobStatusValueText[JobStatusTileCount];
static ModernCard *jobStatusNameCard = nullptr, *jobStatusProgressCard = nullptr, *jobStatusThumbnailCard = nullptr;
static StaticTextField *jobStatusNameField = nullptr, *jobStatusProgressField = nullptr;
static StaticTextField *jobStatusLayersField = nullptr, *jobStatusTimeField = nullptr;
static String<72> jobStatusNameText;
static String<16> jobStatusProgressText, jobStatusLayersText, jobStatusTimeText;
static ModernTextButton *jobStatusPauseResumeButton = nullptr, *jobStatusAbortButton = nullptr;
static DrawDirect *jobStatusThumbnail = nullptr;
static PopupWindow *jobStatusConfirmPopup = nullptr;
static ModernTextButton *jobStatusConfirmTitle = nullptr;
static String<32> jobStatusConfirmText;
enum class JobStatusConfirmAction : uint8_t { None, Pause, Resume, Abort };
static JobStatusConfirmAction jobStatusConfirmAction = JobStatusConfirmAction::None;
static float jobStatusHeaterTemps[JobStatusMaxHeaters] = { 0.0f };
static bool jobStatusHeaterValid[JobStatusMaxHeaters] = { false };
static OM::HeaterStatus jobStatusHeaterStatus[JobStatusMaxHeaters] = { OM::HeaterStatus::off };
static constexpr unsigned int JobStatusMaxExtruders = 8;
static float jobStatusFilamentDiameter[JobStatusMaxExtruders] = { 0.0f };
static bool jobStatusFilamentDiameterValid[JobStatusMaxExtruders] = { false };
static float jobStatusRequestedSpeed = 0.0f, jobStatusTopSpeed = 0.0f, jobStatusExtrusionRate = 0.0f;
static unsigned int jobStatusLayer = 0, jobStatusNumLayers = 0, jobStatusProgress = 0;
static uint32_t jobStatusDuration = 0;
static uint32_t jobStatusLastLiveRefresh = 0;

static constexpr unsigned int TuneToolsPerPage = 4;
static constexpr unsigned int TuneMaxExtruders = 8;
static constexpr unsigned int TuneMaxFans = 16;
static unsigned int tuneToolPage = 0;

static ModernTextButton *tuneSpeedButton = nullptr;
static ModernTextButton *tuneGeneralFanButtons[2] = { nullptr, nullptr };
static StaticTextField *tuneGeneralFanLabels[2] = { nullptr, nullptr };
static int8_t tuneGeneralFanIndices[2] = { -1, -1 };
static ModernTextButton *tuneToolNumberButtons[TuneToolsPerPage] = { nullptr };
static ModernTextButton *tuneToolFanButtons[TuneToolsPerPage] = { nullptr };
static ModernTextButton *tuneToolFlowButtons[TuneToolsPerPage] = { nullptr };
static ModernTextButton *tuneToolPaButtons[TuneToolsPerPage] = { nullptr };
static ModernTextButton *tuneZOffsetButton = nullptr;
static ModernIconButton *tunePageUpButton = nullptr, *tunePageDownButton = nullptr;

static String<8> tuneToolNumberText[TuneToolsPerPage];
static String<12> tuneToolFanText[TuneToolsPerPage];
static String<12> tuneToolFlowText[TuneToolsPerPage];
static String<16> tuneToolPaText[TuneToolsPerPage];
static String<12> tuneSpeedText;
static String<12> tuneGeneralFanText[2];
static String<16> tuneZOffsetText;

static int tuneExtruderFactor[TuneMaxExtruders] = { 100, 100, 100, 100, 100, 100, 100, 100 };
static float tunePressureAdvance[TuneMaxExtruders] = { 0.0f };
static bool tunePressureAdvanceValid[TuneMaxExtruders] = { false };
static int tuneFanPercent[TuneMaxFans] = { 0 };
static bool tuneFanValid[TuneMaxFans] = { false };
static String<16> tuneFanNames[TuneMaxFans];
static int tuneSpeedPercent = 100;

static PopupWindow *tunePressureAdvancePopup = nullptr;
static PopupWindow *tuneFanPopup = nullptr;
static PopupWindow *tuneFeedRatePopup = nullptr;
static PopupWindow *tuneSpeedPopup = nullptr;
static ModernTextButton *tunePressureAdvancePopupTitle = nullptr;
static ModernTextButton *tuneFanPopupTitle = nullptr;
static ModernTextButton *tuneFeedRatePopupTitle = nullptr;
static ModernTextButton *tuneSpeedPopupTitle = nullptr;
static ModernTextButton *tunePressureAdvancePopupValue = nullptr;
static ModernTextButton *tuneFanPopupValue = nullptr;
static ModernTextButton *tuneFeedRatePopupValue = nullptr;
static ModernTextButton *tuneSpeedPopupValue = nullptr;
static String<32> tunePopupTitleText;
static String<16> tunePopupValueText;

enum class TunePopupKind : uint8_t { None, Speed, Fan, Flow, PressureAdvance };
static TunePopupKind tunePopupKind = TunePopupKind::None;
static int tunePopupResource = -1;
static int tunePopupPercent = 0;
static float tunePopupPa = 0.0f;
#else
static constexpr unsigned int StatusObjectsPerPage = 6;
static unsigned int statusObjectPage = 0;
static unsigned int selectedStatusObject = 0;
#endif

// Register a field as belonging to a UI page.
// We keep this temporarily while migrating away from the old
// per-page field arrays.
static void RegisterField(DisplayField *arr[], size_t &count, DisplayField *f)
{
	if (f != nullptr && count < 64)
	{
		arr[count++] = f;
	}
}

// Legacy group show/hide helper.
// Keep this for now while the new UiPage system is being introduced.
static void ShowGroup(DisplayField *arr[], size_t count, bool show)
{
	for (size_t i = 0; i < count; ++i)
	{
		mgr.Show(arr[i], show);
	}
}

// Show one of the new UI pages.
//
// Fields with UiPage::None are left alone so the existing PanelDue
// interface continues to work while we migrate it to the new system.
static void ShowUiPage(UiPage page)
{
	currentUiPage = page;

	for (DisplayField *field = mgr.GetRoot(); field != nullptr; field = field->next)
	{
		const UiPage fieldPage = field->GetUiPage();

		if (fieldPage != UiPage::None)
		{
			field->Show(fieldPage == page);
		}
	}

	mgr.Refresh(true);
}

// -- END: UI page handling

static float axisMaxVal = 0.0;
static FloatField *controlTabAxisPos[MaxDisplayableAxes];
#if DISPLAY_X == 800
static FloatField *printTabAxisPos[MaxDisplayableAxes];
#endif
static FloatField *movePopupAxisPos[MaxDisplayableAxes];
static FloatField *currentTemps[MaxSlots];
static FloatField *fpHeightField, *fpLayerHeightField, *babystepOffsetField;
static TextButtonWithLabel *babystepMinusButton, *babystepPlusButton;
static IntegerField *fpSizeField, *fpFilamentField, *filePopupTitleField;
static ProgressBar *printProgressBar;
static SingleButton *tabControl, *tabStatus, *tabSystem;
static ButtonBase *filesButton, *pauseButton, *resumeButton, *cancelButton, *babystepButton, *reprintButton;
static TextField *timeLeftField, *zProbe;
static TextField *fpNameField, *fpGeneratedByField, *fpLastModifiedField, *fpPrintTimeField;
DrawDirect *fpThumbnail;
static StaticTextField *moveAxisRows[MaxDisplayableAxes];
static StaticTextField *nameField, *statusField;
static StaticTextField *screensaverText;
static IntegerButton *activeTemps[MaxSlots], *standbyTemps[MaxSlots];
static IntegerButton *spd, *extrusionFactors[MaxSlots], *fanSpeed, *baudRateButton, *volumeButton, *infoTimeoutButton, *screensaverTimeoutButton, *feedrateAmountButton;
static TextButton *languageButton, *coloursButton, *dimmingTypeButton, *heaterCombiningButton, *logLevelButton;
static TextButtonWithLabel *babystepAmountButton;
static SingleButton *moveButton, *extrudeButton, *macroButton;
static PopupWindow *babystepPopup;
static AlertPopup *alertPopup;
static CharButtonRow *keyboardRows[4];
static const char* _ecv_array const * _ecv_array currentKeyboard;
static void (*keyboardDataHandler)(const char *data) = nullptr;

constexpr PixelNumber masterTabWidth = (DISPLAY_X == 480) ? 84 : 132;
constexpr PixelNumber contentLeft = masterTabWidth + margin;
constexpr PixelNumber contentWidth = DisplayX - contentLeft - margin;
constexpr PixelNumber contentTop = buttonHeight;

static bool IsMasterTab(const DisplayField *field)
{
	return field == tabControl || field == tabStatus || field == tabSystem;
}

// Move full-screen legacy fields into the content pane to the right of the master rail.
static void RelayoutLegacyFields()
{
	DisplayField *seen[512];
	size_t seenCount = 0;
#if DISPLAY_X == 800
	// STATUS > OBJECT is a native 800x480 modern page and is already positioned
	// in the content pane. Do not apply the legacy full-screen relayout twice.
	DisplayField * const roots[] = { controlRoot, printRoot, messageRoot, setupRoot };
#else
	DisplayField * const roots[] = { controlRoot, printRoot, statusObjectsRoot, messageRoot, setupRoot };
#endif

	for (DisplayField *root : roots)
	{
		for (DisplayField *field = root; field != nullptr; field = field->next)
		{
			bool alreadySeen = false;
			for (size_t i = 0; i < seenCount; ++i)
			{
				if (seen[i] == field)
				{
					alreadySeen = true;
					break;
				}
			}
			if (alreadySeen)
			{
				continue;
			}

			_ecv_assert(seenCount < ARRAY_SIZE(seen));
			seen[seenCount++] = field;
			if (IsMasterTab(field))
			{
				continue;
			}

			const PixelNumber oldX = field->GetMinX();
			const PixelNumber oldWidth = field->GetMaxX() - oldX + 1;
			const PixelNumber newX = contentLeft + static_cast<PixelNumber>((static_cast<uint32_t>(oldX) * contentWidth) / DisplayX);
			PixelNumber newWidth = static_cast<PixelNumber>((static_cast<uint32_t>(oldWidth) * contentWidth) / DisplayX);
			if (newWidth == 0)
			{
				newWidth = 1;
			}

			field->SetPositionAndWidth(newX, newWidth);
			field->SetPosition(newX, field->GetMinY() + contentTop);
		}
	}
}

static TextButton *AddTopTab(unsigned int index, unsigned int count, const char *label, Event event)
{
	const PixelNumber width = contentWidth / count;
	TextButton * const tab = new TextButton(0, contentLeft + index * width, width, label, event);
	mgr.AddField(tab);
	return tab;
}

static ButtonBase * null currentTab = nullptr;

static ButtonPress currentButton;
static ButtonPress fieldBeingAdjusted;
static ButtonPress currentExtrudeRatePress, currentExtrudeAmountPress;

static String<machineNameLength> machineName;
static String<printingFileLength> printingFile;
static bool lastJobFileNameAvailable = false;
static String<zprobeBufLength> zprobeBuf;
static String<generatedByTextLength> generatedByText;
static String<lastModifiedTextLength> lastModifiedText;
static String<printTimeTextLength> printTimeText;

const size_t maxUserCommandLength = 40;					// max length of a user gcode command
const size_t numUserCommandBuffers = 6;					// number of command history buffers plus one

static String<maxUserCommandLength> userCommandBuffers[numUserCommandBuffers];
static size_t currentUserCommandBuffer = 0, currentHistoryBuffer = 0;

static unsigned int numToolColsUsed = 0;
static unsigned int numHeaterAndToolColumns = 0;
static int oldIntValue;
static Event eventToConfirm = evNull;
static uint8_t numVisibleAxes = 0;						// initialise to 0 so we refresh the macros list when we receive the number of axes
static uint8_t numDisplayedAxes = 0;
static bool isDelta = false;

const char* _ecv_array null currentFile = nullptr;			// file whose info is displayed in the file info popup
const StringTable * strings = &LanguageTables[0];
static bool keyboardIsDisplayed = false;
static bool keyboardShifted = false;

int32_t alertMode = -1;									// the mode of the current alert, or -1 if no alert displayed
uint32_t alertTicks = 0;
uint32_t infoTimeout = DefaultInfoTimeout;				// info timeout in seconds, 0 means don't display into messages at all
uint32_t whenAlertReceived;
bool displayingResponse = false;						// true if displaying a response

static PixelNumber screensaverTextWidth = 0;
static uint32_t lastScreensaverMoved = 0;

static uint8_t currentWorkplaceNumber = OM::MaxTotalWorkplaces;
static int8_t currentTool = -2;							// Initialized to a value never returned by RRF to have the logic for "no tool" applied at startup
static bool allAxesHomed = false;
static const bool isLandscape = true; 					// Once portrait mode is enabled, this needs to be de-const-ed

#ifdef SUPPORT_ENCODER

# include "Hardware/RotaryEncoder.hpp"

static RotaryEncoder *encoder;
static uint32_t lastEncoderCommandSentAt = 0;
#endif

inline PixelNumber CalcWidth(unsigned int numCols, PixelNumber displayWidth = DisplayX)
{
	return (displayWidth - 2 * margin + fieldSpacing)/numCols - fieldSpacing;
}

inline PixelNumber CalcXPos(unsigned int col, PixelNumber width, int offset = 0)
{
	return col * (width + fieldSpacing) + margin + offset;
}

// Add a text button with a string parameter
TextButton *AddTextButton(PixelNumber row, unsigned int col, unsigned int numCols, const char* _ecv_array text, Event evt, const char* param, PixelNumber displayWidth = DisplayX)
{
	PixelNumber width = CalcWidth(numCols, displayWidth);
	PixelNumber xpos = CalcXPos(col, width);
	TextButton *f = new TextButton(row - 2, xpos, width, text, evt, param);
	mgr.AddField(f);
	return f;
}

// Add a text button with an int parameter
TextButton *AddTextButton(PixelNumber row, unsigned int col, unsigned int numCols, const char* _ecv_array text, Event evt, int param, PixelNumber displayWidth = DisplayX)
{
	PixelNumber width = CalcWidth(numCols, displayWidth);
	PixelNumber xpos = CalcXPos(col, width);
	TextButton *f = new TextButton(row - 2, xpos, width, text, evt, param);
	mgr.AddField(f);
	return f;
}

// Add an integer button
IntegerButton *AddIntegerButton(PixelNumber row, unsigned int col, unsigned int numCols, const char * _ecv_array null label, const char * _ecv_array null units, Event evt, PixelNumber displayWidth = DisplayX)
{
	PixelNumber width = CalcWidth(numCols, displayWidth);
	PixelNumber xpos = CalcXPos(col, width);
	IntegerButton *f = new IntegerButton(row - 2, xpos, width, label, units);
	f->SetEvent(evt, 0);
	mgr.AddField(f);
	return f;
}

// Add an icon button with a string parameter
IconButton *AddIconButton(PixelNumber row, unsigned int col, unsigned int numCols, Icon icon, Event evt, const char* param, PixelNumber displayWidth = DisplayX)
{
	PixelNumber width = CalcWidth(numCols, displayWidth);
	PixelNumber xpos = CalcXPos(col, width);
	IconButton *f = new IconButton(row - 2, xpos, width, icon, evt, param);
	mgr.AddField(f);
	return f;
}

// Add an icon button with an int parameter
IconButton *AddIconButton(PixelNumber row, unsigned int col, unsigned int numCols, Icon icon, Event evt, int param, PixelNumber displayWidth = DisplayX)
{
	PixelNumber width = CalcWidth(numCols, displayWidth);
	PixelNumber xpos = CalcXPos(col, width);
	IconButton *f = new IconButton(row - 2, xpos, width, icon, evt, param);
	mgr.AddField(f);
	return f;
}

// Add an icon button with a string parameter
IconButtonWithText *AddIconButtonWithText(PixelNumber row, unsigned int col, unsigned int numCols, Icon icon, Event evt, const char * text, const char* param, PixelNumber displayWidth = DisplayX)
{
	PixelNumber width = CalcWidth(numCols, displayWidth);
	PixelNumber xpos = CalcXPos(col, width);
	IconButtonWithText *f = new IconButtonWithText(row - 2, xpos, width, icon, evt, text, param);
	mgr.AddField(f);
	return f;
}

// Add an icon button with an int parameter
IconButtonWithText *AddIconButtonWithText(PixelNumber row, unsigned int col, unsigned int numCols, Icon icon, Event evt, int intVal, const int param, PixelNumber displayWidth = DisplayX)
{
	PixelNumber width = CalcWidth(numCols, displayWidth);
	PixelNumber xpos = CalcXPos(col, width);
	IconButtonWithText *f = new IconButtonWithText(row - 2, xpos, width, icon, evt, intVal, param);
	mgr.AddField(f);
	return f;
}

// Add an icon button with an int parameter
IconButtonWithText *AddIconButtonWithText(PixelNumber row, unsigned int col, unsigned int numCols, Icon icon, Event evt, const char * text, const int param, PixelNumber displayWidth = DisplayX)
{
	PixelNumber width = CalcWidth(numCols, displayWidth);
	PixelNumber xpos = CalcXPos(col, width);
	IconButtonWithText *f = new IconButtonWithText(row - 2, xpos, width, icon, evt, text, param);
	mgr.AddField(f);
	return f;
}

// Create a row of text buttons.
// Optionally, set one to 'pressed' and return that one.
// Set the colours before calling this
ButtonPress CreateStringButtonRow(
		Window * parentWindow,
		PixelNumber top,
		PixelNumber left,
		PixelNumber totalWidth,
		PixelNumber spacing,
		unsigned int numButtons,
		const char* _ecv_array const text[],
		const char* _ecv_array const params[],
		Event evt,
		int selected = -1,
		bool textButtonForAxis = false,
		DisplayField** firstButton = nullptr)
{
	const PixelNumber step = (totalWidth + spacing)/numButtons;
	ButtonPress bp;
	// Since Window->AddField prepends fields in the linked list we start with the last element
	for (int i = numButtons - 1; i >= 0; --i)
	{
		TextButton *tp =
				textButtonForAxis
				? new TextButtonForAxis(top, left + i * step, step - spacing, text[i], evt, params[i])
				: new TextButton(		top, left + i * step, step - spacing, text[i], evt, params[i]);
		parentWindow->AddField(tp);
		if ((int)i == selected)
		{
			tp->Press(true, 0);
			bp = ButtonPress(tp, 0);
		}
		if (firstButton != nullptr && i == 0)
		{
			*firstButton = tp;
		}
	}
	return bp;
}

#if 0  // currently unused
// Create a row of icon buttons.
// Set the colours before calling this
void CreateIconButtonRow(Window * pf, PixelNumber top, PixelNumber left, PixelNumber totalWidth, PixelNumber spacing, unsigned int numButtons,
									const Icon icons[], const char* _ecv_array const params[], Event evt)
{
	const PixelNumber step = (totalWidth + spacing)/numButtons;
	for (unsigned int i = 0; i < numButtons; ++i)
	{
		pf->AddField(new IconButton(top, left + i * step, step - spacing, icons[i], evt, params[i]));
	}
}
#endif

// Create a popup bar with string parameters
PopupWindow *CreateStringPopupBar(const ColourScheme& colours, PixelNumber width, unsigned int numEntries, const char* const text[], const char* const params[], Event ev)
{
	PopupWindow *pf = new PopupWindow(popupBarHeight, width, colours.popupBackColour, colours.popupBorderColour);
	DisplayField::SetDefaultColours(colours.popupButtonTextColour, colours.popupButtonBackColour);
	PixelNumber step = (width - 2 * popupSideMargin + popupFieldSpacing)/numEntries;
	for (unsigned int i = 0; i < numEntries; ++i)
	{
		pf->AddField(new TextButton(popupTopMargin, popupSideMargin + i * step, step - popupFieldSpacing, text[i], ev, params[i]));
	}
	return pf;
}

// Create a popup bar with integer parameters
// If the 'params' parameter is null then we use 0, 1, 2.. at the parameters
PopupWindow *CreateIntPopupBar(const ColourScheme& colours, PixelNumber width, unsigned int numEntries, const char* const text[], const int * null params, Event ev, Event zeroEv)
{
	PopupWindow *pf = new PopupWindow(popupBarHeight, width, colours.popupBackColour, colours.popupBorderColour);
	DisplayField::SetDefaultColours(colours.popupButtonTextColour, colours.popupButtonBackColour);
	PixelNumber step = (width - 2 * popupSideMargin + popupFieldSpacing)/numEntries;
	for (unsigned int i = 0; i < numEntries; ++i)
	{
		const int iParam = (params == nullptr) ? (int)i : params[i];
		pf->AddField(new TextButton(popupSideMargin, popupSideMargin + i * step, step - popupFieldSpacing, text[i], (params && params[i] == 0) ? zeroEv : ev, iParam));
	}
	return pf;
}

// Nasty hack to work around bug in RepRapFirmware 1.09k and earlier
// The M23 and M30 commands don't work if we send the full path, because "0:/gcodes/" gets prepended regardless.
const char * _ecv_array StripPrefix(const char * _ecv_array dir)
{
	if (GetFirmwareFeatures().IsBitSet(noGcodesFolder))			// if running RepRapFirmware
	{
		const size_t len = strlen(dir);
		if (len >= 8 && memcmp(dir, "/gcodes/", 8) == 0)
		{
			dir += 8;
		}
		else if (len >= 10 && memcmp(dir, "0:/gcodes/", 10) == 0)
		{
			dir += 10;
		}
		else if (strcmp(dir, "/gcodes") == 0 || strcmp(dir, "0:/gcodes") == 0)
		{
			dir += len;
		}
	}
	return dir;
}


static void SendGcode(const char *data)
{
	SerialIo::Sendf("%s\n", data);
}

static void PopupEditData(const char *data)
{
	alertPopup->UpdateData(data);
	dbg("received data %s\n", data);
	mgr.ClearPopup(true, keyboardPopup);
}

// Adjust the brightness
static void ChangeBrightness(bool up)
{
	int adjust = max<int>(1, nvData.GetBrightness() / 5);
	if (!up)
	{
		adjust = -adjust;
	}
	SetBrightness(nvData.GetBrightness() + adjust);
}


void UI::SetAxisMin(size_t index, float val)
{
	if (index >= MaxTotalAxes)
	{
		return;
	}
#if DISPLAY_X == 800
	statusObjectAxisMin[index] = val;
	statusObjectAxisMinValid[index] = true;
	statusObjectsDirty = true;
#endif
}

void UI::SetAxisMax(size_t index, float val)
{
	if (index >= MaxTotalAxes)
	{
		return;
	}

	axisMaxVal = max(axisMaxVal, val);
#if DISPLAY_X == 800
	statusObjectAxisMax[index] = val;
	statusObjectAxisMaxValid[index] = true;
	statusObjectsDirty = true;
#endif
}


// Cycle through available display dimmer types
static void ChangeDisplayDimmerType()
{
	DisplayDimmerType newType = (DisplayDimmerType) ((uint8_t)nvData.GetDisplayDimmerType() + 1);
	if (newType == DisplayDimmerType::NumTypes)
	{
		newType = (DisplayDimmerType)0;
	}
	nvData.SetDisplayDimmerType(newType);
}

// Cyce through available heater combine types and repaint
static void ChangeHeaterCombineType()
{
	HeaterCombineType newType = (HeaterCombineType) ((uint8_t)nvData.GetHeaterCombineType() + 1);
	if (newType == HeaterCombineType::NumTypes)
	{
		newType = (HeaterCombineType)0;
	}
	nvData.SetHeaterCombineType(newType);
	UI::AllToolsSeen();
}

// Update an integer field, provided it isn't the one being adjusted
// Don't update it if the value hasn't changed, because that makes the display flicker unnecessarily
static void UpdateField(IntegerButton *f, int val)
{
	if (f != fieldBeingAdjusted.GetButton() && f->GetValue() != val)
	{
		f->SetValue(val);
	}
}

static void PopupAreYouSure(Event ev, const char* text, const char* query = strings->areYouSure)
{
	eventToConfirm = ev;
	if (isLandscape)
	{
		areYouSureTextField->SetValue(text);
		areYouSureQueryField->SetValue(query);
		mgr.SetPopup(areYouSurePopup, AutoPlace, AutoPlace);
	}
}

static void CreateIntegerAdjustPopup(const ColourScheme& colours)
{
	// Create the popup window used to adjust temperatures, fan speed, extrusion factor etc.
	static const char* const tempPopupText[] = {"-5", "-1", strings->set, "+1", "+5"};
	static const int tempPopupParams[] = { -5, -1, 0, 1, 5 };
	setTempPopup = CreateIntPopupBar(colours, tempPopupBarWidth, 5, tempPopupText, tempPopupParams, evAdjustInt, evSetInt);
}

static void CreateIntegerRPMAdjustPopup(const ColourScheme& colours)
{
	// Create the popup window used to adjust temperatures, fan speed, extrusion factor etc.
	static const char* const rpmPopupText[] = {"-1000", "-100", "-10", strings->set, "+10", "+100", "+1000"};
	static const int rpmPopupParams[] = { -1000, -100, -10, 0, 10, 100, 1000 };
	setRPMPopup = CreateIntPopupBar(colours, rpmPopupBarWidth, 7, rpmPopupText, rpmPopupParams, evAdjustInt, evSetInt);
}

// Create the movement popup window
static void CreateMovePopup(const ColourScheme& colours)
{
	static const char * _ecv_array const xyJogValues[] = { "-100", "-10", "-1", "-0.1", "0.1",  "1", "10", "100" };
	static const char * _ecv_array const zJogValues[] = { "-50", "-5", "-0.5", "-0.05", "0.05",  "0.5", "5", "50" };

	movePopup = new StandardPopupWindow(movePopupHeight, movePopupWidth, colours.popupBackColour, colours.popupBorderColour, colours.popupTextColour, colours.buttonImageBackColour, strings->moveHead);
	PixelNumber ypos = popupTopMargin + buttonHeight + moveButtonRowSpacing;
	const PixelNumber axisPosYpos = ypos + (MaxDisplayableAxes - 1) * (buttonHeight + moveButtonRowSpacing);
	const PixelNumber xpos = popupSideMargin + axisLabelWidth;
	PixelNumber column = popupSideMargin + margin;
	PixelNumber xyFieldWidth = (DISPLAY_X - (2 * margin) - (MaxDisplayableAxes * fieldSpacing))/(MaxDisplayableAxes + 1);

	for (size_t i = 0; i < MaxDisplayableAxes; ++i)
	{
		DisplayField::SetDefaultColours(colours.popupButtonTextColour, colours.popupButtonBackColour);
		const char * _ecv_array const * _ecv_array values = (axisNames[i][0] == 'Z') ? zJogValues : xyJogValues;
		CreateStringButtonRow(movePopup, ypos, xpos, movePopupWidth - xpos - popupSideMargin, fieldSpacing, 8, values, values, evMoveAxis, -1, true);

		// We create the label after the button row, so that the buttons follow it in the field order, which makes it easier to hide them
		DisplayField::SetDefaultColours(colours.popupTextColour, colours.popupBackColour);
		StaticTextField * const tf = new StaticTextField(ypos + labelRowAdjust, popupSideMargin, axisLabelWidth, TextAlignment::Left, axisNames[i]);
		movePopup->AddField(tf);
		moveAxisRows[i] = tf;
		UI::ShowAxis(i, i < MIN_AXES, axisNames[i]);

		DisplayField::SetDefaultColours(colours.popupTextColour, colours.popupInfoBackColour);
		FloatField *f = new FloatField(axisPosYpos, column, xyFieldWidth, TextAlignment::Left, 2, axisNames[i]);
		movePopupAxisPos[i] = f;
		movePopup->AddField(f);
		f->Show(i < MIN_AXES);
		column += xyFieldWidth + fieldSpacing;

		ypos += buttonHeight + moveButtonRowSpacing;
	}
}

// Create the extrusion controls popup
static void CreateExtrudePopup(const ColourScheme& colours)
{
	static const char * _ecv_array extrudeAmountValues[] = { "100", "50", "20", "10", "5",  "1" };
	static const char * _ecv_array extrudeSpeedValues[] = { "50", "20", "10", "5", "2", "1", "0.5" };
	static const char * _ecv_array extrudeSpeedParams[] = { "3000", "1200", "600", "300", "120", "60", "30" };		// must be extrudeSpeedValues * 60

	extrudePopup = new StandardPopupWindow(extrudePopupHeight, extrudePopupWidth, colours.popupBackColour, colours.popupBorderColour, colours.popupTextColour, colours.buttonImageBackColour, strings->extrusionAmount);
	PixelNumber ypos = popupTopMargin + buttonHeight + extrudeButtonRowSpacing;
	DisplayField::SetDefaultColours(colours.popupButtonTextColour, colours.popupButtonBackColour);
	currentExtrudeAmountPress = CreateStringButtonRow(extrudePopup, ypos, popupSideMargin, extrudePopupWidth - 2 * popupSideMargin, fieldSpacing, 6, extrudeAmountValues, extrudeAmountValues, evExtrudeAmount, 3);
	ypos += buttonHeight + extrudeButtonRowSpacing;
	DisplayField::SetDefaultColours(colours.popupTextColour, colours.popupBackColour);
	extrudePopup->AddField(new StaticTextField(ypos + labelRowAdjust, popupSideMargin, extrudePopupWidth - 2 * popupSideMargin, TextAlignment::Centre, strings->extrusionSpeed));
	ypos += buttonHeight + extrudeButtonRowSpacing;
	DisplayField::SetDefaultColours(colours.popupButtonTextColour, colours.popupButtonBackColour);
	currentExtrudeRatePress = CreateStringButtonRow(
			extrudePopup,
			ypos,
			popupSideMargin,
			extrudePopupWidth - 2 * popupSideMargin,
			fieldSpacing,
			ARRAY_SIZE(extrudeSpeedValues),
			extrudeSpeedValues,
			extrudeSpeedParams,
			evExtrudeRate,
			ARRAY_SIZE(extrudeSpeedValues) / 2);

	ypos += buttonHeight + extrudeButtonRowSpacing;
	extrudePopup->AddField(new TextButton(ypos, popupSideMargin, extrudePopupWidth/3 - 2 * popupSideMargin, strings->extrude, evExtrude));
	extrudePopup->AddField(new TextButton(ypos, (2 * extrudePopupWidth)/3 + popupSideMargin, extrudePopupWidth/3 - 2 * popupSideMargin, strings->retract, evRetract));
}

// Create a popup used to list files pr macros
PopupWindow *CreateFileListPopup(FileListButtons& controlButtons, TextButton ** _ecv_array fileButtons, unsigned int numRows, unsigned int numCols, const ColourScheme& colours, bool filesNotMacros,
		PixelNumber popupHeight = fileListPopupHeight, PixelNumber popupWidth = fileListPopupWidth)
pre(fileButtons.lim == numRows * numCols)
{
	PopupWindow * const popup = new StandardPopupWindow(popupHeight, popupWidth, colours.popupBackColour, colours.popupBorderColour, colours.popupTextColour, colours.buttonImageBackColour, nullptr);
	const PixelNumber closeButtonPos = popupWidth - closeButtonWidth - popupSideMargin;
	const PixelNumber navButtonWidth = (closeButtonPos - popupSideMargin)/7;
	const PixelNumber upButtonPos = closeButtonPos - navButtonWidth - fieldSpacing;
	const PixelNumber rightButtonPos = upButtonPos - navButtonWidth - fieldSpacing;
	const PixelNumber leftButtonPos = rightButtonPos - navButtonWidth - fieldSpacing;
	const PixelNumber textPos = popupSideMargin + navButtonWidth;
	const PixelNumber changeButtonPos = popupSideMargin;

	DisplayField::SetDefaultColours(colours.popupTextColour, colours.popupBackColour);
	if (filesNotMacros)
	{
		popup->AddField(filePopupTitleField = new IntegerField(popupTopMargin + labelRowAdjust, textPos, leftButtonPos - textPos, TextAlignment::Centre, strings->filesOnCard, nullptr));
		popup->AddField(fileListPopupNoFiles = new StaticTextField(popupHeight / 2 - popupTopMargin, popupSideMargin, popupWidth, TextAlignment::Centre, strings->noFilesFound));
		fileListPopupNoFiles->Show(false);
	}
	else
	{
		popup->AddField(new StaticTextField(popupTopMargin + labelRowAdjust, textPos, leftButtonPos - textPos, TextAlignment::Centre, strings->macros));
	}

	DisplayField::SetDefaultColours(colours.popupButtonTextColour, colours.buttonImageBackColour);
	if (filesNotMacros)
	{
		popup->AddField(changeCardButton = new IconButton(popupTopMargin, changeButtonPos, navButtonWidth, IconFiles, evChangeCard, 0));
	}

	const Event scrollEvent = (filesNotMacros) ? evScrollFiles : evScrollMacros;

	DisplayField::SetDefaultColours(colours.popupButtonTextColour, colours.popupButtonBackColour);
	popup->AddField(controlButtons.scrollLeftButton = new TextButton(popupTopMargin, leftButtonPos, navButtonWidth, LEFT_ARROW, scrollEvent, -1));
	controlButtons.scrollLeftButton->Show(false);
	popup->AddField(controlButtons.scrollRightButton = new TextButton(popupTopMargin, rightButtonPos, navButtonWidth, RIGHT_ARROW, scrollEvent, 1));
	controlButtons.scrollRightButton->Show(false);
	popup->AddField(controlButtons.folderUpButton = new TextButton(popupTopMargin, upButtonPos, navButtonWidth, UP_ARROW, (filesNotMacros) ? evFilesUp : evMacrosUp));
	controlButtons.folderUpButton->Show(false);

	const PixelNumber fileFieldWidth = (popupWidth + fieldSpacing - (2 * popupSideMargin))/numCols;
	for (unsigned int c = 0; c < numCols; ++c)
	{
		PixelNumber row = popupTopMargin;
		for (unsigned int r = 0; r < numRows; ++r)
		{
			row += buttonHeight + fileButtonRowSpacing;
			TextButton *t = new TextButton(row, (fileFieldWidth * c) + popupSideMargin, fileFieldWidth - fieldSpacing, nullptr, evNull);
			t->Show(false);
			popup->AddField(t);
			*fileButtons = t;
			++fileButtons;
		}
	}

	controlButtons.errorField = new IntegerField(popupTopMargin + 2 * (buttonHeight + fileButtonRowSpacing), popupSideMargin, popupWidth - (2 * popupSideMargin),
							TextAlignment::Centre, strings->error, strings->accessingSdCard);
	controlButtons.errorField->Show(false);
	popup->AddField(controlButtons.errorField);
	return popup;
}

static void ThumbnailRefreshNotify(bool full, bool changed)
{
	UNUSED(changed);

	if (!full || !currentFile)
		return;

	dbg("full %d changed %d currentFile %s\n", full, changed, currentFile);
	SerialIo::Sendf(GetFirmwareFeatures().IsBitSet(noM20M36) ? "M408 S36 P" : "M36 ");			// ask for the file info
	SerialIo::SendFilename(CondStripDrive(FileManager::GetFilesDir()), currentFile);
	SerialIo::SendChar('\n');
}

// Create the popup window used to display the file dialog
static void CreateFileActionPopup(const ColourScheme& colours)
{
	PixelNumber y_start, height;
	PixelNumber x_start, width;

	fileDetailPopup = new StandardPopupWindow(fileInfoPopupHeight, fileInfoPopupWidth, colours.popupBackColour, colours.popupBorderColour, colours.popupTextColour, colours.buttonImageBackColour, nullptr);
	DisplayField::SetDefaultColours(colours.popupTextColour, colours.popupBackColour);

	PixelNumber ypos = popupTopMargin + 1;
	fpNameField = new TextField(ypos, popupSideMargin, fileInfoPopupWidth - closeButtonWidth - 3 * popupSideMargin, TextAlignment::Left, strings->fileName);
	ypos += rowTextHeight + 3;
	fpGeneratedByField = new TextField(ypos, popupSideMargin, fileInfoPopupWidth - 2 * popupSideMargin, TextAlignment::Left, strings->generatedBy, generatedByText.c_str());
	ypos += rowTextHeight;

	y_start = ypos + 3;
	height = 7 * rowTextHeight + (2 * rowTextHeight) / 3;

	x_start = fileInfoPopupWidth - popupSideMargin * 3 / 2 - fileInfoPopupWidth / 3;
	width = fileInfoPopupWidth / 3 + 5;

	fpThumbnail = new DrawDirect(y_start, x_start, height, width, ThumbnailRefreshNotify);

	dbg("y_start %d x_start %d height %d width %d\n", y_start, x_start, height, width);
	dbg("text height %d\n", rowTextHeight);

	fpSizeField = new IntegerField(ypos, popupSideMargin, fileInfoPopupWidth - 2 * popupSideMargin - fileInfoPopupWidth / 3, TextAlignment::Left, strings->fileSize, " b");
	ypos += rowTextHeight;
	fpLayerHeightField = new FloatField(ypos, popupSideMargin, fileInfoPopupWidth - 2 * popupSideMargin - fileInfoPopupWidth / 3, TextAlignment::Left, 2, strings->layerHeight, "mm");
	ypos += rowTextHeight;
	fpHeightField = new FloatField(ypos, popupSideMargin, fileInfoPopupWidth - 2 * popupSideMargin - fileInfoPopupWidth / 3, TextAlignment::Left, 1, strings->objectHeight, "mm");
	ypos += rowTextHeight;
	fpFilamentField = new IntegerField(ypos, popupSideMargin, fileInfoPopupWidth - 2 * popupSideMargin - fileInfoPopupWidth / 3, TextAlignment::Left, strings->filamentNeeded, "mm");
	ypos += rowTextHeight;
	fpLastModifiedField = new TextField(ypos, popupSideMargin, fileInfoPopupWidth - 2 * popupSideMargin - fileInfoPopupWidth / 3, TextAlignment::Left, strings->lastModified, lastModifiedText.c_str());
	ypos += rowTextHeight;
	fpPrintTimeField = new TextField(ypos, popupSideMargin, fileInfoPopupWidth - 2 * popupSideMargin - fileInfoPopupWidth / 3, TextAlignment::Left, strings->estimatedPrintTime, printTimeText.c_str());
	fileDetailPopup->AddField(fpNameField);
	fileDetailPopup->AddField(fpSizeField);
	fileDetailPopup->AddField(fpLayerHeightField);
	fileDetailPopup->AddField(fpHeightField);
	fileDetailPopup->AddField(fpFilamentField);
	fileDetailPopup->AddField(fpGeneratedByField);
	fileDetailPopup->AddField(fpLastModifiedField);
	fileDetailPopup->AddField(fpPrintTimeField);
	fileDetailPopup->AddField(fpThumbnail);

	// Add the buttons
	DisplayField::SetDefaultColours(colours.popupButtonTextColour, colours.popupButtonBackColour);
	fileDetailPopup->AddField(new TextButton(popupTopMargin + 10 * rowTextHeight, popupSideMargin, fileInfoPopupWidth/3 - 2 * popupSideMargin, strings->print, evPrintFile));
	fileDetailPopup->AddField(new TextButton(popupTopMargin + 10 * rowTextHeight, fileInfoPopupWidth/3 + popupSideMargin, fileInfoPopupWidth/3 - 2 * popupSideMargin, strings->simulate, evSimulateFile));
	fileDetailPopup->AddField(new IconButton(popupTopMargin + 10 * rowTextHeight, (2 * fileInfoPopupWidth)/3 + popupSideMargin, fileInfoPopupWidth/3 - 2 * popupSideMargin, IconTrash, evDeleteFile));
}

// Create the "Are you sure?" popup
static void CreateAreYouSurePopup(const ColourScheme& colours)
{
	areYouSurePopup = new PopupWindow(areYouSurePopupHeight, areYouSurePopupWidth, colours.popupBackColour, colours.popupBorderColour);
	DisplayField::SetDefaultColours(colours.popupTextColour, colours.popupBackColour);
	areYouSurePopup->AddField(areYouSureTextField = new StaticTextField(popupSideMargin, margin, areYouSurePopupWidth - 2 * margin, TextAlignment::Centre, nullptr));
	areYouSurePopup->AddField(areYouSureQueryField = new StaticTextField(popupTopMargin + rowHeight, margin, areYouSurePopupWidth - 2 * margin, TextAlignment::Centre, nullptr));

	DisplayField::SetDefaultColours(colours.popupButtonTextColour, colours.popupButtonBackColour);
	areYouSurePopup->AddField(new IconButton(popupTopMargin + 2 * rowHeight, popupSideMargin, areYouSurePopupWidth/2 - 2 * popupSideMargin, IconOk, evYes));
	areYouSurePopup->AddField(new IconButton(popupTopMargin + 2 * rowHeight, areYouSurePopupWidth/2 + 10, areYouSurePopupWidth/2 - 2 * popupSideMargin, IconCancel, evCancel));
}

static void CreateScreensaverPopup()
{
	screensaverPopup = new PopupWindow(max(DisplayX, DisplayY), max(DisplayX, DisplayY), black, black, false);
	DisplayField::SetDefaultColours(white, black);
	static const char * text = "Touch to wake up";
	screensaverTextWidth = DisplayField::GetTextWidth(text, DisplayX);
	screensaverPopup->AddField(screensaverText = new StaticTextField(row1, margin, screensaverTextWidth, TextAlignment::Left, text));
}

static void CreateFirmwareUpdatePopup()
{
	firmwareUpdatePopup = new PopupWindow(max(DisplayX, DisplayY), max(DisplayX, DisplayY), black, black, false);
	DisplayField::SetDefaultColours(white, black);
	static const char * text = "Updating firmware";
	const int textWidth = DisplayField::GetTextWidth(text, DisplayX);
	firmwareUpdatePopup->AddField(new StaticTextField(DisplayY/2-rowHeight/2, DisplayX/2-textWidth/2, textWidth, TextAlignment::Left, text));
}

// Create the baud rate adjustment popup
static void CreateBaudRatePopup(const ColourScheme& colours)
{
	static const char* const baudPopupText[] = { "9600", "19200", "38400", "57600", "115200" };
	static const int baudPopupParams[] = { 9600, 19200, 38400, 57600, 115200 };
	baudPopup = CreateIntPopupBar(colours, fullPopupWidth, 5, baudPopupText, baudPopupParams, evAdjustBaudRate, evAdjustBaudRate);
}

// Create the volume adjustment popup
static void CreateVolumePopup(const ColourScheme& colours)
{
	static_assert(Buzzer::MaxVolume == 5, "MaxVolume assumed to be 5 here");
	static const char* const volumePopupText[Buzzer::MaxVolume + 1] = { "0", "1", "2", "3", "4", "5" };
	volumePopup = CreateIntPopupBar(colours, fullPopupWidth, ARRAY_SIZE(volumePopupText), volumePopupText, nullptr, evAdjustVolume, evAdjustVolume);
}

// Create the volume adjustment popup
static void CreateInfoTimeoutPopup(const ColourScheme& colours)
{
	static const char* const infoTimeoutPopupText[Buzzer::MaxVolume + 1] = { "0", "2", "5", "10" };
	static const int values[] = { 0, 2, 5, 10 };
	infoTimeoutPopup = CreateIntPopupBar(colours, fullPopupWidth, ARRAY_SIZE(infoTimeoutPopupText), infoTimeoutPopupText, values, evAdjustInfoTimeout, evAdjustInfoTimeout);
}

// Create the screensaver timeout adjustment popup
static void CreateScreensaverTimeoutPopup(const ColourScheme& colours)
{
	static const char* const screensaverTimeoutPopupText[Buzzer::MaxVolume + 1] = { "off", "60", "120", "180", "240", "300" };
	static const int values[] = { 0, 60, 120, 180, 240, 300 };
	screensaverTimeoutPopup = CreateIntPopupBar(colours, fullPopupWidth, ARRAY_SIZE(screensaverTimeoutPopupText), screensaverTimeoutPopupText, values, evAdjustScreensaverTimeout, evAdjustScreensaverTimeout);
}

// Create the babystep amount adjustment popup
static void CreateBabystepAmountPopup(const ColourScheme& colours)
{
	static const int values[] = { 0, 1, 2, 3 };
	babystepAmountPopup = CreateIntPopupBar(colours, fullPopupWidth, ARRAY_SIZE(babystepAmounts), babystepAmounts, values, evAdjustBabystepAmount, evAdjustBabystepAmount);
}

// Create the feedrate amount adjustment popup
static void CreateFeedrateAmountPopup(const ColourScheme& colours)
{
	static const char* const feedrateText[] = {"600", "1200", "2400", "6000", "12000"};
	static const int values[] = { 600, 1200, 2400, 6000, 12000 };
	feedrateAmountPopup = CreateIntPopupBar(colours, fullPopupWidth, ARRAY_SIZE(feedrateText), feedrateText, values, evAdjustFeedrate, evAdjustFeedrate);
}

// Create the colour scheme change popup
static void CreateColoursPopup(const ColourScheme& colours)
{
	if (NumColourSchemes >= 2)
	{
		// Put all the colour scheme names in a single _ecv_array for the call to CreateIntPopupBar
		const char* coloursPopupText[NumColourSchemes];
		for (size_t i = 0; i < NumColourSchemes; ++i)
		{
			coloursPopupText[i] = strings->colourSchemeNames[i];
		}
		coloursPopup = CreateIntPopupBar(colours, fullPopupWidth, NumColourSchemes, coloursPopupText, nullptr, evAdjustColours, evAdjustColours);
	}
	else
	{
		coloursPopup = nullptr;
	}
}

// Create the language popup (currently only affects the keyboard layout)
static void CreateLanguagePopup(const ColourScheme& colours)
{
	languagePopup = new PopupWindow(popupBarHeight, fullPopupWidth, colours.popupBackColour, colours.popupBorderColour);
	DisplayField::SetDefaultColours(colours.popupButtonTextColour, colours.popupButtonBackColour);
	PixelNumber step = (fullPopupWidth - 2 * popupSideMargin + popupFieldSpacing)/NumLanguages;
	for (unsigned int i = 0; i < NumLanguages; ++i)
	{
		languagePopup->AddField(new TextButton(popupSideMargin, popupSideMargin + i * step, step - popupFieldSpacing, LanguageTables[i].languageName, evAdjustLanguage, i));
	}
}

// Create the pop-up keyboard
static void CreateKeyboardPopup(uint32_t language, ColourScheme colours)
{
	static const char* _ecv_array const keysEN[8] = { "1234567890-+", "QWERTYUIOP[]", "ASDFGHJKL:@", "ZXCVBNM,./", "!\"#$%^&*()_=", "qwertyuiop{}", "asdfghjkl;'", "zxcvbnm<>?" };
	static const char* _ecv_array const keysDE[8] = { "1234567890-+", "QWERTZUIOP[]", "ASDFGHJKL:@", "YXCVBNM,./", "!\"#$%^&*()_=", "qwertzuiop{}", "asdfghjkl;'", "yxcvbnm<>?" };
	static const char* _ecv_array const keysFR[8] = { "1234567890-+", "AZERTWUIOP[]", "QSDFGHJKLM@", "YXCVBN.,:/", "!\"#$%^&*()_=", "azertwuiop{}", "qsdfghjklm'", "yxcvbn<>;?" };
	static const char* _ecv_array const * const keyboards[] = {
			keysEN,	// English
			keysDE,	// German
			keysFR,	// French
			keysEN,	// Spanish
			keysEN,	// Czech
			keysEN,	// Italian
			keysEN,	// Dutch
			keysEN,	// Polish
#if USE_CYRILLIC_CHARACTERS
			keysEN,	// Ukrainian
			keysEN,	// Russian
#elif USE_JAPANESE_CHARACTERS
			keysEN, // Japanese
#endif
	};

	static_assert(ARRAY_SIZE(keyboards) >= NumLanguages, "Wrong number of keyboard entries");

	keyboardPopup = new StandardPopupWindow(keyboardPopupHeight, keyboardPopupWidth, colours.popupBackColour, colours.popupBorderColour, colours.popupInfoTextColour, colours.buttonImageBackColour, nullptr, keyboardTopMargin);

	// Add the text area in which the command is built
	DisplayField::SetDefaultColours(colours.popupInfoTextColour, colours.popupInfoBackColour);		// need a different background colour
	userCommandField = new TextField(keyboardTopMargin + labelRowAdjust, popupSideMargin, keyboardPopupWidth - 2 * popupSideMargin - closeButtonWidth - popupFieldSpacing, TextAlignment::Left, nullptr, "_");
	userCommandField->SetLabel(userCommandBuffers[currentUserCommandBuffer].c_str());	// set up to display the current user command
	keyboardPopup->AddField(userCommandField);

	if (language >= NumLanguages)
	{
		language = 0;
	}

	currentKeyboard = keyboards[language];
	PixelNumber row = keyboardTopMargin + keyButtonVStep;

	for (size_t i = 0; i < 4; ++i)
	{
		DisplayField::SetDefaultColours(colours.popupButtonTextColour, colours.popupButtonBackColour);
		// New code using CharButtonRow to economise on RAM at the expense of more flash memory usage
		const PixelNumber column = popupSideMargin + (i * keyButtonHStep)/3;
		keyboardRows[i] = new CharButtonRow(row, column, keyButtonWidth, keyButtonHStep, currentKeyboard[i], evKey);
		keyboardPopup->AddField(keyboardRows[i]);
		DisplayField::SetDefaultColours(colours.popupButtonTextColour, colours.buttonImageBackColour);
		switch (i)
		{
		case 0:
			keyboardPopup->AddField(new IconButton(row, keyboardPopupWidth - popupSideMargin - (5 * keyButtonWidth)/4, (5 * keyButtonWidth)/4, IconBackspace, evBackspace));
			break;

		case 2:
			keyboardPopup->AddField(new TextButton(row, keyboardPopupWidth - popupSideMargin - (3 * keyButtonWidth)/2, (3 * keyButtonWidth)/2, UP_ARROW, evUp));
			break;

		case 3:
			keyboardPopup->AddField(new TextButton(row, keyboardPopupWidth - popupSideMargin - (3 * keyButtonWidth)/2, (3 * keyButtonWidth)/2, DOWN_ARROW, evDown));
			break;

		default:
			break;
		}
		row += keyButtonVStep;
	}

	// Add the shift, space and enter keys
	const PixelNumber keyButtonHSpace = keyButtonHStep - keyButtonWidth;
	const PixelNumber wideKeyButtonWidth = (keyboardPopupWidth - 2 * popupSideMargin - 2 * keyButtonHSpace)/5;
	DisplayField::SetDefaultColours(colours.popupButtonTextColour, colours.popupButtonBackColour);
	keyboardPopup->AddField(new TextButton(row, popupSideMargin, wideKeyButtonWidth, "Shift", evShift, 0));
	keyboardPopup->AddField(new TextButton(row, popupSideMargin + wideKeyButtonWidth + keyButtonHSpace, 2 * wideKeyButtonWidth, "", evKey, (int)' '));
	DisplayField::SetDefaultColours(colours.popupButtonTextColour, colours.buttonImageBackColour);
	keyboardPopup->AddField(new IconButton(row, popupSideMargin + 3 * wideKeyButtonWidth + 2 * keyButtonHSpace, wideKeyButtonWidth, IconEnter, evSendKeyboardCommand));

	keyboardDataHandler = SendGcode;
}

// Create the babystep popup
static void CreateBabystepPopup(const ColourScheme& colours)
{
	babystepPopup = new StandardPopupWindow(babystepPopupHeight, babystepPopupWidth, colours.popupBackColour, colours.popupBorderColour, colours.popupTextColour, colours.buttonImageBackColour,
			strings->babyStepping);
	PixelNumber ypos = popupTopMargin + babystepRowSpacing;
	DisplayField::SetDefaultColours(colours.popupTextColour, colours.popupBackColour);
	babystepPopup->AddField(babystepOffsetField = new FloatField(ypos, popupSideMargin, babystepPopupWidth - 2 * popupSideMargin, TextAlignment::Left, 3, strings->currentZoffset, "mm"));
	ypos += babystepRowSpacing;
	DisplayField::SetDefaultColours(colours.popupTextColour, colours.buttonImageBackColour);
	const PixelNumber width = CalcWidth(2, babystepPopupWidth - 2 * popupSideMargin);
	babystepPopup->AddField(babystepMinusButton = new TextButtonWithLabel(ypos, CalcXPos(0, width, popupSideMargin), width, babystepAmounts[nvData.GetBabystepAmountIndex()], evBabyStepMinus, nullptr, LESS_ARROW " "));
	babystepPopup->AddField(babystepPlusButton = new TextButtonWithLabel(ypos, CalcXPos(1, width, popupSideMargin), width, babystepAmounts[nvData.GetBabystepAmountIndex()], evBabyStepPlus, nullptr, MORE_ARROW " "));
}

// Create the grid of heater icons and temperatures
static void CreateTemperatureGrid(const ColourScheme& colours)
{
	// Add the emergency stop button
	DisplayField::SetDefaultColours(colours.stopButtonTextColour, colours.stopButtonBackColour);
	mgr.AddField(new TextButton(row2, margin, bedColumn - fieldSpacing - margin - 16, strings->stop, evEmergencyStop));

	// Add the labels and the debug field
	DisplayField::SetDefaultColours(colours.labelTextColour, colours.defaultBackColour);
	mgr.AddField(debugField = new StaticTextField(row1 + labelRowAdjust, margin, bedColumn - fieldSpacing - margin, TextAlignment::Left, "debug"));
	mgr.AddField(new StaticTextField(row3 + labelRowAdjust, margin, bedColumn - fieldSpacing - margin, TextAlignment::Right, strings->current));
	mgr.AddField(new StaticTextField(row4 + labelRowAdjust, margin, bedColumn - fieldSpacing - margin, TextAlignment::Right, strings->active));
	mgr.AddField(new StaticTextField(row5 + labelRowAdjust, margin, bedColumn - fieldSpacing - margin, TextAlignment::Right, strings->standby));

	// Add the grid
	for (unsigned int i = 0; i < MaxSlots; ++i)
	{
		const PixelNumber column = ((tempButtonWidth + fieldSpacing) * i) + bedColumn;

		// Add the icon button
		DisplayField::SetDefaultColours(colours.buttonTextColour, colours.buttonImageBackColour);
		IconButtonWithText * const b = new IconButtonWithText(row2, column, tempButtonWidth, i == 0 ? IconBed : IconNozzle, evSelectHead, i, i);
		b->Show(false);
		toolButtons[i] = b;
		mgr.AddField(b);

		// Add the current temperature field
		DisplayField::SetDefaultColours(colours.infoTextColour, colours.defaultBackColour);
		FloatField * const f = new FloatField(row3 + labelRowAdjust, column, tempButtonWidth, TextAlignment::Centre, 1);
		f->Show(false);
		currentTemps[i] = f;
		mgr.AddField(f);

		// Add the active temperature button
		DisplayField::SetDefaultColours(colours.buttonTextColour, colours.buttonTextBackColour);
		IntegerButton *ib = new IntegerButton(row4, column, tempButtonWidth);
		ib->SetEvent(evAdjustToolActiveTemp, i);
		ib->SetValue(0);
		ib->Show(false);
		activeTemps[i] = ib;
		mgr.AddField(ib);

		// Add the standby temperature button
		ib = new IntegerButton(row5, column, tempButtonWidth);
		ib->SetEvent(evAdjustToolStandbyTemp, i);
		ib->SetValue(0);
		ib->Show(false);
		standbyTemps[i] = ib;
		mgr.AddField(ib);
	}
}

// Create the extra fields for the Control tab
static void CreateControlTabFields(const ColourScheme& colours)
{
	mgr.SetRoot(commonRoot);

	DisplayField::SetDefaultColours(colours.infoTextColour, colours.infoBackColour);
	PixelNumber column = margin;
	PixelNumber xyFieldWidth = (DISPLAY_X - (2 * margin) - (MaxDisplayableAxes * fieldSpacing))/(MaxDisplayableAxes + 1);
	for (size_t i = 0; i < MaxDisplayableAxes; ++i)
	{
		FloatField * const f = new FloatField(row6p3 + labelRowAdjust, column, xyFieldWidth, TextAlignment::Left, 2, axisNames[i]);
		controlTabAxisPos[i] = f;
		mgr.AddField(f);
		f->Show(i < MIN_AXES);
		column += xyFieldWidth + fieldSpacing;
	}
	zprobeBuf[0] = 0;
	mgr.AddField(zProbe = new TextField(row6p3 + labelRowAdjust, column, DISPLAY_X - column - margin, TextAlignment::Left, "P", zprobeBuf.c_str()));

	DisplayField::SetDefaultColours(colours.buttonTextColour, colours.notHomedButtonBackColour);
	homeAllButton = AddIconButton(row7p7, 0, MaxDisplayableAxes + 2, IconHomeAll, evSendCommand, "G28");
	homeButtons[0] = AddIconButtonWithText(row7p7, 1, MaxDisplayableAxes + 2, IconHomeAll, evHomeAxis, axisNames[0], axisNames[0]);
	homeButtons[1] = AddIconButtonWithText(row7p7, 2, MaxDisplayableAxes + 2, IconHomeAll, evHomeAxis, axisNames[1], axisNames[1]);
	homeButtons[2] = AddIconButtonWithText(row7p7, 3, MaxDisplayableAxes + 2, IconHomeAll, evHomeAxis, axisNames[2], axisNames[2]);
#if MaxDisplayableAxes > 3
	homeButtons[3] = AddIconButtonWithText(row7p7, 4, MaxDisplayableAxes + 2, IconHomeAll, evHomeAxis, axisNames[3], axisNames[3]);
	homeButtons[3]->Show(false);
#endif
#if MaxDisplayableAxes > 4
	homeButtons[4] = AddIconButtonWithText(row7p7, 5, MaxDisplayableAxes + 2, IconHomeAll, evHomeAxis, axisNames[4], axisNames[4]);
	homeButtons[4]->Show(false);
#endif
#if MaxDisplayableAxes > 5
	homeButtons[5] = AddIconButtonWithText(row7p7, 6, MaxDisplayableAxes + 2, IconHomeAll, evHomeAxis, axisNames[5], axisNames[5]);
	homeButtons[5]->Show(false);
#endif
	DisplayField::SetDefaultColours(colours.buttonTextColour, colours.buttonImageBackColour);
	bedCompButton = AddIconButton(row7p7, MaxDisplayableAxes + 1, MaxDisplayableAxes + 2, IconBedComp, evSendCommand, "G32");

	filesButton = AddIconButton(row8p7, 0, 4, IconFiles, evListFiles, nullptr);
	DisplayField::SetDefaultColours(colours.buttonTextColour, colours.buttonTextBackColour);
	moveButton = AddTextButton(row8p7, 1, 4, strings->move, evMovePopup, nullptr);
	extrudeButton = AddTextButton(row8p7, 2, 4, strings->extrusion, evExtrudePopup, nullptr);
	macroButton = AddTextButton(row8p7, 3, 4, strings->macro, evListMacros, nullptr);

	// When there is room, we also display a few macro buttons on the right hand side
	for (size_t i = 0; i < NumControlPageMacroButtons; ++i)
	{
		// The position and width of the buttons will get corrected when we know how many tools we have
		TextButton * const b = controlPageMacroButtons[i] = new TextButton(row2 + i * rowHeight, 999, 99, nullptr, evNull);
		b->Show(false);			// hide them until we have loaded the macros
		mgr.AddField(b);
	}

	controlRoot = mgr.GetRoot();
}

// Create the fields for the Printing tab
static void CreatePrintingTabFields(const ColourScheme& colours)
{
	mgr.SetRoot(commonRoot);

	DisplayField::SetDefaultColours(colours.buttonTextColour, colours.buttonTextBackColour);
	mgr.AddField(new TextButton(row6, margin, bedColumn - fieldSpacing - margin, "OBJECTS", evStatusObjects));

	// Extrusion factor buttons
	DisplayField::SetDefaultColours(colours.buttonTextColour, colours.buttonTextBackColour);
	for (unsigned int i = 0; i < MaxSlots; ++i)
	{
		const PixelNumber column = ((tempButtonWidth + fieldSpacing) * i) + bedColumn;

		IntegerButton * const ib = new IntegerButton(row6, column, tempButtonWidth);
		ib->SetValue(100);
		ib->SetEvent(evExtrusionFactor, i);
		ib->Show(false);
		extrusionFactors[i] = ib;
		mgr.AddField(ib);
	}

	// Speed button
	mgr.AddField(spd = new IntegerButton(row7, speedColumn, stateColumnWdith - fieldSpacing, strings->speed, "%"));
	spd->SetValue(100);
	spd->SetEvent(evAdjustSpeed, "M220 S");

	// Fan button
	mgr.AddField(fanSpeed = new IntegerButton(row7, fanColumn, stateColumnWdith - fieldSpacing, strings->fan, "%"));
	fanSpeed->SetEvent(evAdjustFan, 0);
	fanSpeed->SetValue(0);

	DisplayField::SetDefaultColours(colours.buttonTextColour, colours.buttonTextBackColour);
	babystepButton = new TextButton(row7, babystepColumn, stateColumnWdith - fieldSpacing, strings->babystep, evBabyStepPopup);
	mgr.AddField(babystepButton);

	DisplayField::SetDefaultColours(colours.buttonTextColour, colours.resetButtonBackColour);
	cancelButton = new TextButton(row7, cancelColumn, stateColumnWdith - fieldSpacing, strings->cancel, evReset, "M0");
	mgr.AddField(cancelButton);

	DisplayField::SetDefaultColours(colours.buttonTextColour, colours.pauseButtonBackColour);
	pauseButton = new TextButton(row7, pauseColumn, stateColumnWdith - (2 * margin), strings->pause, evPausePrint, "M25");
	mgr.AddField(pauseButton);

	DisplayField::SetDefaultColours(colours.buttonTextColour, colours.resumeButtonBackColour);
	resumeButton = new TextButton(row7, resumeColumn, stateColumnWdith - (2 * margin), strings->resume, evResumePrint, "M24");
	mgr.AddField(resumeButton);

#if DISPLAY_X == 800
	// On 5" and 7" screens there is room to show the current position on the Print page
	const PixelNumber offset = rowHeight - 20;
	DisplayField::SetDefaultColours(colours.infoTextColour, colours.infoBackColour);
	PixelNumber column = margin;
	PixelNumber xyFieldWidth = (DISPLAY_X - (2 * margin) - (MaxDisplayableAxes * fieldSpacing))/(MaxDisplayableAxes + 1);
	for (size_t i = 0; i < MaxDisplayableAxes; ++i)
	{
		FloatField * const f = new FloatField(row8 + labelRowAdjust - 4, column, xyFieldWidth, TextAlignment::Left, 2, axisNames[i]);
		printTabAxisPos[i] = f;
		mgr.AddField(f);
		f->Show(i < MIN_AXES);
		column += xyFieldWidth + fieldSpacing;
	}
#else
	const PixelNumber offset = 0;
#endif

	DisplayField::SetDefaultColours(colours.buttonTextColour, colours.buttonTextBackColour);
	const PixelNumber reprintRow =
#if DISPLAY_X == 800
			row9
#else
			row8
#endif
			;
	reprintButton = new TextButton(reprintRow, speedColumn, 2 * stateColumnWdith - fieldSpacing, strings->reprint, evReprint);
	reprintButton->Show(false);
	mgr.AddField(reprintButton);

	DisplayField::SetDefaultColours(colours.progressBarColour,colours. progressBarBackColour);
	mgr.AddField(printProgressBar = new ProgressBar(row8 + offset + (rowHeight - progressBarHeight)/2, margin, progressBarHeight, DisplayX - 2 * margin));
	mgr.Show(printProgressBar, false);

	DisplayField::SetDefaultColours(colours.labelTextColour, colours.defaultBackColour);
	mgr.AddField(timeLeftField = new TextField(row9 + offset, margin, DisplayX - 2 * margin, TextAlignment::Left, strings->timeRemaining));
	mgr.Show(timeLeftField, false);

	printRoot = mgr.GetRoot();
}

static void AddStatusSubTabs(DisplayField *&root);
#if DISPLAY_X == 800
static PixelNumber ObjectX(PixelNumber svgX);
static PixelNumber ObjectW(PixelNumber svgW);
#endif

// Create the Status > Object subpage.
#if DISPLAY_X == 800
static void GetStatusObjectDisplayName(unsigned int index, String<32>& out)
{
	if (index < StatusMaxObjects && !statusObjects[index].name.IsEmpty())
	{
		out.copy(statusObjects[index].name.c_str());
	}
	else
	{
		out.printf("Object %u", index + 1);
	}
}

static bool GetStatusObjectBedBounds(float& xMin, float& xMax, float& yMin, float& yMax)
{
	if (statusObjectXAxis < 0 || statusObjectYAxis < 0 ||
		statusObjectXAxis >= static_cast<int>(MaxTotalAxes) || statusObjectYAxis >= static_cast<int>(MaxTotalAxes) ||
		!statusObjectAxisMinValid[statusObjectXAxis] || !statusObjectAxisMaxValid[statusObjectXAxis] ||
		!statusObjectAxisMinValid[statusObjectYAxis] || !statusObjectAxisMaxValid[statusObjectYAxis])
	{
		return false;
	}
	xMin = statusObjectAxisMin[statusObjectXAxis];
	xMax = statusObjectAxisMax[statusObjectXAxis];
	yMin = statusObjectAxisMin[statusObjectYAxis];
	yMax = statusObjectAxisMax[statusObjectYAxis];
	return xMax > xMin && yMax > yMin;
}

static void RefreshStatusObjectMap()
{
	if (statusObjectMap == nullptr)
	{
		return;
	}

	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour neutral = UTFT::fromRGB(195, 202, 212);
	const Colour accent = UTFT::fromRGB(226, 69, 63);
	const Colour cancelledFill = UTFT::fromRGB(192, 57, 47);
	const Colour cancelledText = UTFT::fromRGB(58, 15, 12);

	float xMin = 0.0f, xMax = 0.0f, yMin = 0.0f, yMax = 0.0f;
	const bool boundsValid = GetStatusObjectBedBounds(xMin, xMax, yMin, yMax);
	statusObjectMap->SetBounds(xMin, xMax, yMin, yMax, boundsValid);
	// Clearing the canvas here also removes markers that moved or disappeared.
	statusObjectMap->SetChanged();

	PixelNumber bedX = 0, bedY = 0, bedW = 0, bedH = 0;
	statusObjectMap->GetBedBounds(bedX, bedY, bedW, bedH);
	PixelNumber placedX[StatusMaxObjects] = { 0 };
	PixelNumber placedY[StatusMaxObjects] = { 0 };
	unsigned int placedCount = 0;
	static const int8_t jitterX[] = { 0, 10, -10, 10, -10, 16, -16, 0, 0 };
	static const int8_t jitterY[] = { 0, -10, 10, 10, -10, 0, 0, 16, -16 };

	for (unsigned int i = 0; i < StatusMaxObjects; ++i)
	{
		ModernTextButton * const marker = statusObjectMarkers[i];
		if (marker == nullptr)
		{
			continue;
		}
		const StatusObjectInfo& obj = statusObjects[i];
		if (i >= statusObjectCount || !obj.present || !obj.xValid || !obj.yValid || !boundsValid)
		{
			mgr.Show(marker, false);
			continue;
		}

		const float objectX = (obj.xMin + obj.xMax) * 0.5f;
		const float objectY = (obj.yMin + obj.yMax) * 0.5f;
		PixelNumber centreX = 0, centreY = 0;
		if (!statusObjectMap->Project(objectX, objectY, centreX, centreY))
		{
			mgr.Show(marker, false);
			continue;
		}

		// Apply a very small deterministic offset when two numbered markers overlap.
		int bestX = static_cast<int>(centreX);
		int bestY = static_cast<int>(centreY);
		for (unsigned int attempt = 0; attempt < ARRAY_SIZE(jitterX); ++attempt)
		{
			const int candidateX = static_cast<int>(centreX) + jitterX[attempt];
			const int candidateY = static_cast<int>(centreY) + jitterY[attempt];
			bool overlaps = false;
			for (unsigned int p = 0; p < placedCount; ++p)
			{
				const int dx = candidateX - static_cast<int>(placedX[p]);
				const int dy = candidateY - static_cast<int>(placedY[p]);
				if (dx > -28 && dx < 28 && dy > -28 && dy < 28)
				{
					overlaps = true;
					break;
				}
			}
			bestX = candidateX;
			bestY = candidateY;
			if (!overlaps)
			{
				break;
			}
		}

		const int markerSize = 32;
		int left = bestX - markerSize / 2;
		int top = bestY - markerSize / 2;
		const int minLeft = static_cast<int>(bedX);
		const int maxLeft = static_cast<int>(bedX + bedW) - markerSize;
		const int minTop = static_cast<int>(bedY);
		const int maxTop = static_cast<int>(bedY + bedH) - markerSize;
		if (left < minLeft) left = minLeft;
		if (left > maxLeft) left = maxLeft;
		if (top < minTop) top = minTop;
		if (top > maxTop) top = maxTop;
		if (left < 0) left = 0;
		if (top < 0) top = 0;
		marker->SetPosition(static_cast<PixelNumber>(left), static_cast<PixelNumber>(top));
		statusObjectMarkerText[i].printf("%u", i + 1);
		marker->SetText(statusObjectMarkerText[i].c_str());

		const bool selected = selectedStatusObject == static_cast<int>(i);
		if (obj.cancelled)
		{
			marker->SetColours(cancelledText, cancelledFill);
			marker->SetBorderVisible(false);
		}
		else
		{
			marker->SetColours(selected ? accent : neutral, tile);
			marker->SetBorderVisible(true);
			marker->SetBorderColour(selected ? accent : neutral);
		}
		marker->SetChanged();
		mgr.Show(marker, true);
		placedX[placedCount] = static_cast<PixelNumber>(bestX);
		placedY[placedCount] = static_cast<PixelNumber>(bestY);
		++placedCount;
	}
}

static void RefreshStatusObjectRows()
{
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour accent = UTFT::fromRGB(226, 69, 63);
	const Colour cancelledFill = UTFT::fromRGB(192, 57, 47);
	const Colour cancelledText = UTFT::fromRGB(58, 15, 12);

	for (unsigned int row = 0; row < StatusObjectsPerPage; ++row)
	{
		const unsigned int index = statusObjectPage * StatusObjectsPerPage + row;
		ModernTextButton * const number = statusObjectNumberButtons[row];
		ModernTextButton * const name = statusObjectNameButtons[row];
		if (index >= statusObjectCount || index >= StatusMaxObjects || !statusObjects[index].present)
		{
			mgr.Show(number, false);
			mgr.Show(name, false);
			continue;
		}

		statusObjectRowNumberText[row].printf("%u", index + 1);
		GetStatusObjectDisplayName(index, statusObjectRowNameText[row]);
		number->SetText(statusObjectRowNumberText[row].c_str());
		name->SetText(statusObjectRowNameText[row].c_str());
		const bool selected = selectedStatusObject == static_cast<int>(index);
		const bool cancelled = statusObjects[index].cancelled;
		number->SetEvent(cancelled ? evNull : evStatusObjectNumber, static_cast<int>(row));

		if (cancelled)
		{
			number->SetColours(cancelledText, cancelledFill);
			number->SetBorderVisible(false);       // semantic red takes precedence over Accent
		}
		else
		{
			number->SetColours(selected ? accent : text, tile);
			number->SetBorderVisible(selected);
			number->SetBorderColour(accent);
		}
		name->SetColours(text, tile);
		name->SetBorderVisible(selected);
		name->SetBorderColour(accent);
		mgr.Show(number, true);
		mgr.Show(name, true);
	}

	mgr.Show(statusObjectPageUpButton, statusObjectPage > 0);
	mgr.Show(statusObjectPageDownButton,
		(statusObjectPage + 1) * StatusObjectsPerPage < statusObjectCount &&
		(statusObjectPage + 1) * StatusObjectsPerPage < StatusMaxObjects);
}

static void RefreshStatusObjectsPage()
{
	const unsigned int maxPage = (statusObjectCount == 0) ? 0 : (statusObjectCount - 1) / StatusObjectsPerPage;
	if (statusObjectPage > maxPage)
	{
		statusObjectPage = maxPage;
		statusObjectsNeedFullRefresh = true;
	}
	if (selectedStatusObject >= static_cast<int>(statusObjectCount))
	{
		selectedStatusObject = -1;
	}
	RefreshStatusObjectRows();
	RefreshStatusObjectMap();
}

static void SelectStatusObject(unsigned int index, bool revealPage)
{
	if (index >= statusObjectCount || index >= StatusMaxObjects || !statusObjects[index].present)
	{
		return;
	}
	const unsigned int oldPage = statusObjectPage;
	selectedStatusObject = static_cast<int>(index);
	if (revealPage)
	{
		statusObjectPage = index / StatusObjectsPerPage;
	}
	RefreshStatusObjectsPage();
	if (statusObjectPage != oldPage)
	{
		mgr.Refresh(true);
	}
	else
	{
		mgr.Refresh(false);
	}
}

static void OpenStatusObjectCancelPopup(unsigned int index)
{
	if (index >= statusObjectCount || index >= StatusMaxObjects || !statusObjects[index].present || statusObjects[index].cancelled)
	{
		return;
	}
	pendingStatusObjectCancel = static_cast<int>(index);
	selectedStatusObject = static_cast<int>(index);
	statusObjectCancelNumberText.printf("%u", index + 1);
	GetStatusObjectDisplayName(index, statusObjectCancelNameText);
	statusObjectCancelNumber->SetText(statusObjectCancelNumberText.c_str());
	statusObjectCancelName->SetText(statusObjectCancelNameText.c_str());
	RefreshStatusObjectsPage();
	mgr.Refresh(false);
	mgr.SetPopup(statusObjectCancelPopup, AutoPlace, AutoPlace);
}

static void CreateStatusObjectCancelPopup()
{
	const Colour pageBg = UTFT::fromRGB(18, 22, 28);
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour neutralBorder = UTFT::fromRGB(59, 67, 79);
	const Colour cancelRed = UTFT::fromRGB(226, 69, 63);
	const Colour cancelFill = UTFT::fromRGB(192, 57, 47);
	const Colour confirmGreen = UTFT::fromRGB(164, 214, 94);

	statusObjectCancelPopup = new PopupWindow(460, 610, pageBg, cancelRed);
	DisplayField::SetDefaultFont(DEFAULT_FONT);
	DisplayField::SetDefaultColours(text, pageBg);
	statusObjectCancelPopup->AddField(new StaticTextField(65, 30, 550, TextAlignment::Centre, "CANCEL OBJECT:"));

	DisplayField::SetDefaultColours(text, tile);
	statusObjectCancelNumber = new ModernTextButton(160, 100, 90, 80, "", evNull, 0, DEFAULT_FONT, true);
	statusObjectCancelNumber->SetBorderColour(neutralBorder);
	statusObjectCancelPopup->AddField(statusObjectCancelNumber);
	statusObjectCancelName = new ModernTextButton(160, 210, 300, 80, "", evNull, 0, DEFAULT_FONT, true);
	statusObjectCancelName->SetBorderColour(neutralBorder);
	statusObjectCancelPopup->AddField(statusObjectCancelName);

	DisplayField::SetDefaultColours(text, cancelFill);
	statusObjectCancelPopup->AddField(new ModernIconButton(320, 150, 140, 80, IconCancel, evStatusObjectCancelClose));
	DisplayField::SetDefaultColours(text, confirmGreen);
	statusObjectCancelPopup->AddField(new ModernIconButton(320, 320, 140, 80, IconOk, evStatusObjectCancelConfirm));
	DisplayField::SetDefaultFont(DEFAULT_FONT);
}
#endif

static void CreateStatusObjectsTabFields(const ColourScheme& colours)
{
#if DISPLAY_X == 800
	UNUSED(colours);
	mgr.SetRoot(baseRoot);
	const Colour pageBg = UTFT::fromRGB(18, 22, 28);
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour neutral = UTFT::fromRGB(195, 202, 212);
	const Colour mapBorder = UTFT::fromRGB(59, 67, 79);
	const Colour axes = UTFT::fromRGB(90, 100, 114);

	DisplayField::SetDefaultFont(glcd19x21);
	DisplayField::SetDefaultColours(text, tile);
	for (unsigned int row = 0; row < StatusObjectsPerPage; ++row)
	{
		const PixelNumber y = 85 + row * 62;
		statusObjectNumberButtons[row] = new ModernTextButton(y, ObjectX(118), ObjectW(55), 56, "", evStatusObjectNumber, row, glcd19x21);
		mgr.AddField(statusObjectNumberButtons[row]);
		statusObjectNameButtons[row] = new ModernTextButton(y, ObjectX(181), ObjectW(217), 56, "", evStatusObjectSelect, row, glcd19x21, false, TextAlignment::Left);
		mgr.AddField(statusObjectNameButtons[row]);
		mgr.Show(statusObjectNumberButtons[row], false);
		mgr.Show(statusObjectNameButtons[row], false);
	}

	statusObjectPageUpButton = new ModernIconButton(401, ObjectX(118), ObjectW(137), 46, IconUp, evStatusObjectPageUp);
	statusObjectPageDownButton = new ModernIconButton(401, ObjectX(261), ObjectW(137), 46, IconDown, evStatusObjectPageDown);
	mgr.AddField(statusObjectPageUpButton);
	mgr.AddField(statusObjectPageDownButton);
	mgr.Show(statusObjectPageUpButton, false);
	mgr.Show(statusObjectPageDownButton, false);

	DisplayField::SetDefaultColours(neutral, tile);
	for (unsigned int i = 0; i < StatusMaxObjects; ++i)
	{
		statusObjectMarkerText[i].printf("%u", i + 1);
		statusObjectMarkers[i] = new ModernTextButton(110, ObjectX(472), 32, 32,
			statusObjectMarkerText[i].c_str(), evStatusObjectMarker, i, glcd19x21, true);
		statusObjectMarkers[i]->SetBorderColour(neutral);
		mgr.AddField(statusObjectMarkers[i]);
		mgr.Show(statusObjectMarkers[i], false);
	}

	statusObjectMap = new StatusObjectMapField(75, ObjectX(440), ObjectW(340), 370,
		ObjectX(472), 110, ObjectW(280), 280, pageBg, tile, mapBorder, axes);
	mgr.AddField(statusObjectMap);

	statusObjectsRoot = mgr.GetRoot();
	AddStatusSubTabs(statusObjectsRoot);
	mgr.SetRoot(statusObjectsRoot);
	mgr.AddField(new ModernCard(0, masterTabWidth, DisplayX - masterTabWidth, DisplayY, pageBg, pageBg));
	statusObjectsRoot = mgr.GetRoot();
	DisplayField::SetDefaultFont(DEFAULT_FONT);
	CreateStatusObjectCancelPopup();
	RefreshStatusObjectsPage();
#else
    mgr.SetRoot(baseRoot);

    const PixelNumber listLeft = contentLeft + margin;
    const PixelNumber listWidth = contentWidth * 4 / 10;
    const PixelNumber mapLeft = listLeft + listWidth + margin;
    const PixelNumber mapWidth = DisplayX - mapLeft - margin;

    DisplayField::SetDefaultColours(colours.infoTextColour, colours.defaultBackColour);
    mgr.AddField(new StaticTextField(contentTop + margin, listLeft, listWidth, "OBJECT CANCEL", TextAlignment::Left));

    const PixelNumber firstRow = contentTop + buttonHeight + margin;
    const PixelNumber rowHeight = buttonHeight;
    mgr.AddField(new TextButton(firstRow, listLeft, listWidth, "1  Object 1", evStatusObject1));
    mgr.AddField(new TextButton(firstRow + rowHeight, listLeft, listWidth, "2  Object 2", evStatusObject2));
    mgr.AddField(new TextButton(firstRow + 2 * rowHeight, listLeft, listWidth, "3  Object 3", evStatusObject3));
    mgr.AddField(new TextButton(firstRow + 3 * rowHeight, listLeft, listWidth, "4  Object 4", evStatusObject4));
    mgr.AddField(new TextButton(firstRow + 4 * rowHeight, listLeft, listWidth, "5  Object 5", evStatusObject5));
    mgr.AddField(new TextButton(firstRow + 5 * rowHeight, listLeft, listWidth, "6  Object 6", evStatusObject6));
    mgr.AddField(new TextButton(DisplayY - buttonHeight - margin, listLeft, (listWidth - margin) / 2, "UP", evStatusObjectPageUp));
    mgr.AddField(new TextButton(DisplayY - buttonHeight - margin, listLeft + (listWidth + margin) / 2, (listWidth - margin) / 2, "DOWN", evStatusObjectPageDown));

    DisplayField::SetDefaultColours(colours.infoTextColour, colours.defaultBackColour);
    mgr.AddField(new StaticTextField(contentTop + margin, mapLeft, mapWidth, "TOP VIEW", TextAlignment::Centre));
    statusObjectsRoot = mgr.GetRoot();
#endif
}

// Create the fields for the Message tab
static void CreateMessageTabFields(const ColourScheme& colours)
{
	mgr.SetRoot(baseRoot);
	DisplayField::SetDefaultColours(colours.buttonTextColour, colours.buttonImageBackColour);
	mgr.AddField(new IconButton(margin,  DisplayX - margin - keyboardButtonWidth, keyboardButtonWidth, IconKeyboard, evKeyboard));
	DisplayField::SetDefaultColours(colours.labelTextColour, colours.defaultBackColour);
	mgr.AddField(new StaticTextField(margin + labelRowAdjust, margin, DisplayX - 2 * margin - keyboardButtonWidth, TextAlignment::Centre, strings->messages));
	PixelNumber row = firstMessageRow;
	for (unsigned int r = 0; r < numMessageRows; ++r)
	{
		StaticTextField *t = new StaticTextField(row, margin, messageTimeWidth, TextAlignment::Left, nullptr);
		mgr.AddField(t);
		messageTimeFields[r] = t;
		t = new StaticTextField(row, messageTextX, messageTextWidth, TextAlignment::Left, nullptr);
		mgr.AddField(t);
		messageTextFields[r] = t;
		row += rowTextHeight;
	}
	messageRoot = mgr.GetRoot();
}

// Create the fields for the Setup tab
static void CreateSetupTabFields(uint32_t language, const ColourScheme& colours)
{
	mgr.SetRoot(baseRoot);
	DisplayField::SetDefaultColours(colours.labelTextColour, colours.defaultBackColour);
	// The firmware version field doubles up as an area for displaying debug messages, so make it the full width of the display
	mgr.AddField(fwVersionField = new TextField(row1, margin, DisplayX, TextAlignment::Left, strings->firmwareVersion, VERSION_TEXT));
	mgr.AddField(freeMem = new IntegerField(row2, margin, DisplayX/2 - margin, TextAlignment::Left, "Free RAM: "));
	mgr.AddField(new ColourGradientField(ColourGradientTopPos, ColourGradientLeftPos, ColourGradientWidth, ColourGradientHeight));

	DisplayField::SetDefaultColours(colours.buttonTextColour, colours.buttonTextBackColour);
	baudRateButton = AddIntegerButton(row3, 0, 3, nullptr, " baud", evSetBaudRate);
	baudRateButton->SetValue(nvData.GetBaudRate());
	volumeButton = AddIntegerButton(row3, 1, 3, strings->volume, nullptr, evSetVolume);
	volumeButton->SetValue(nvData.GetVolume());
	languageButton = AddTextButton(row3, 2, 3, LanguageTables[language].languageName, evSetLanguage, nullptr);
	AddTextButton(row4, 0, 3, strings->calibrateTouch, evCalTouch, nullptr);
	AddTextButton(row4, 1, 3, strings->mirrorDisplay, evInvertX, nullptr);
	AddTextButton(row4, 2, 3, strings->invertDisplay, evInvertY, nullptr);
	coloursButton = AddTextButton(row5, 0, 3, strings->colourSchemeNames[colours.index], evSetColours, nullptr);
	AddTextButton(row5, 1, 3, strings->brightnessDown, evDimmer, nullptr);
	AddTextButton(row5, 2, 3, strings->brightnessUp, evBrighter, nullptr);
	dimmingTypeButton = AddTextButton(row6, 0, 3, strings->displayDimmingNames[(unsigned int)nvData.GetDisplayDimmerType()], evSetDimmingType, nullptr);
	infoTimeoutButton = AddIntegerButton(row6, 1, 3, strings->infoTimeout, nullptr, evSetInfoTimeout);
	infoTimeoutButton->SetValue(infoTimeout);
	AddTextButton(row6, 2, 3, strings->clearSettings, evFactoryReset, nullptr);
	screensaverTimeoutButton = AddIntegerButton(row7, 0, 3, strings->screensaverAfter, nullptr, evSetScreensaverTimeout);
	screensaverTimeoutButton->SetValue(nvData.GetScreensaverTimeout() / 1000);

	const PixelNumber width = CalcWidth(3);
	mgr.AddField(babystepAmountButton = new TextButtonWithLabel(row7, CalcXPos(1, width), width, babystepAmounts[nvData.GetBabystepAmountIndex()], evSetBabystepAmount, nullptr, strings->babystepAmount));

	feedrateAmountButton = AddIntegerButton(row7, 2, 3, strings->feedrate, nullptr, evSetFeedrate);
	feedrateAmountButton->SetValue(nvData.GetFeedrate());

	heaterCombiningButton  = AddTextButton(row8, 0, 3, strings->heaterCombineTypeNames[(unsigned int)nvData.GetHeaterCombineType()], evSetHeaterCombineType, nullptr);
	logLevelButton = AddTextButton(row8, 1, 3, strings->logLevelNames[(unsigned int)MessageLog::LogLevelGet()], evSetLogLevel, nullptr);

	DisplayField::SetDefaultColours(colours.labelTextColour, colours.defaultBackColour);
	mgr.AddField(ipAddressField = new TextField(row9, margin, DisplayX/2 - margin, TextAlignment::Left, "IP: ", ipAddress.c_str()));
	setupRoot = mgr.GetRoot();
}

// Create the fields that are displayed on all pages
static void CreateCommonFields(const ColourScheme& colours)
{
	DisplayField::SetDefaultColours(colours.buttonTextColour, colours.buttonTextBackColour, colours.buttonBorderColour, colours.buttonGradColour,
									colours.buttonPressedBackColour, colours.buttonPressedGradColour, colours.pal);
	const PixelNumber masterWidth = masterTabWidth - 2 * margin;
	tabControl = new TextButton(margin, margin, masterWidth, "CONTROL", evTabControl);
	tabStatus = new TextButton(DisplayY/3 - buttonHeight/2, margin, masterWidth, "STATUS", evTabStatus);
	tabSystem = new TextButton((2 * DisplayY)/3 - buttonHeight/2, margin, masterWidth, "SYSTEM", evTabSystem);
	mgr.AddField(tabControl);
	mgr.AddField(tabStatus);
	mgr.AddField(tabSystem);
}

static void AddControlSubTabs()
{
	mgr.SetRoot(controlRoot);
	AddTopTab(0, 4, "TOOLS", evControlTools);
	AddTopTab(1, 4, "MOVE", evControlMovement);
	AddTopTab(2, 4, "EXTRUDE", evControlExtrusion);
	AddTopTab(3, 4, "MACROS", evControlMacros);
	controlRoot = mgr.GetRoot();
}

static void AddStatusSubTabs(DisplayField *&root)
{
	mgr.SetRoot(root);
	AddTopTab(0, 4, "JOB STATUS", evStatusJobStatus);
	AddTopTab(1, 4, "TUNE", evStatusTune);
	AddTopTab(2, 4, "JOB", evStatusJob);
	AddTopTab(3, 4, "OBJECT", evStatusObjects);
	root = mgr.GetRoot();
}

static void AddSystemSubTabs(DisplayField *&root)
{
	mgr.SetRoot(root);
	AddTopTab(0, 3, "CONSOLE", evSystemConsole);
	AddTopTab(1, 3, "ALERTS", evSystemAlerts);
	AddTopTab(2, 3, "SETTINGS", evSystemSettings);
	root = mgr.GetRoot();
}


#if DISPLAY_X == 800
static PixelNumber ControlX(PixelNumber svgX)
{
	return contentLeft + static_cast<PixelNumber>((static_cast<uint32_t>(svgX - 90) * contentWidth) / 710);
}

static PixelNumber ControlW(PixelNumber svgW)
{
	return static_cast<PixelNumber>((static_cast<uint32_t>(svgW) * contentWidth) / 710);
}

static unsigned int CountControlToolResources()
{
	unsigned int count = 0;
	OM::IterateToolsWhile([&count](OM::Tool*&, size_t) { ++count; return true; });
	OM::IterateBedsWhile([&count](OM::Bed*& bed, size_t) {
		if (bed != nullptr && bed->heater >= 0) ++count;
		return true;
	});
	OM::IterateChambersWhile([&count](OM::Chamber*& chamber, size_t) {
		if (chamber != nullptr && chamber->heater >= 0) ++count;
		return true;
	});
	return count;
}

static bool GetControlToolResource(unsigned int wanted, ControlToolResource& result)
{
	result = ControlToolResource{};
	unsigned int pos = 0;
	bool found = false;
	OM::IterateToolsWhile([&](OM::Tool*& tool, size_t) {
		if (pos++ == wanted)
		{
			result.type = ControlToolResourceType::Tool;
			result.index = tool->index;
			result.heater = (tool->heaters[0] != nullptr) ? tool->heaters[0]->heaterIndex : -1;
			found = true;
			return false;
		}
		return true;
	});
	if (found) return true;
	OM::IterateBedsWhile([&](OM::Bed*& bed, size_t) {
		if (bed != nullptr && bed->heater >= 0)
		{
			if (pos++ == wanted)
			{
				result.type = ControlToolResourceType::Bed;
				result.index = bed->index;
				result.heater = bed->heater;
				found = true;
				return false;
			}
		}
		return true;
	});
	if (found) return true;
	OM::IterateChambersWhile([&](OM::Chamber*& chamber, size_t) {
		if (chamber != nullptr && chamber->heater >= 0)
		{
			if (pos++ == wanted)
			{
				result.type = ControlToolResourceType::Chamber;
				result.index = chamber->index;
				result.heater = chamber->heater;
				found = true;
				return false;
			}
		}
		return true;
	});
	return found;
}

static OM::HeaterStatus GetControlToolHeaterStatus(const ControlToolResource& resource)
{
	if (resource.type == ControlToolResourceType::Tool)
	{
		OM::Tool * const tool = OM::GetTool(resource.index);
		if (tool != nullptr)
		{
			if (tool->status == OM::ToolStatus::active) return OM::HeaterStatus::active;
			if (tool->status == OM::ToolStatus::standby) return OM::HeaterStatus::standby;
		}
	}
	// For beds/chambers use the heater-index cache directly. Unlike the legacy
	// slot mapping this also covers resources that only appear on later pages.
	if (resource.heater >= 0 && resource.heater < static_cast<int>(JobStatusMaxHeaters))
	{
		return jobStatusHeaterStatus[resource.heater];
	}
	return OM::HeaterStatus::off;
}

static int GetControlToolTarget(const ControlToolResource& resource, bool active)
{
	if (resource.type == ControlToolResourceType::Tool)
	{
		OM::Tool * const tool = OM::GetTool(resource.index);
		if (tool != nullptr && tool->heaters[0] != nullptr)
		{
			return active ? tool->heaters[0]->activeTemp : tool->heaters[0]->standbyTemp;
		}
	}
	if (resource.heater >= 0 && resource.heater < static_cast<int>(ControlToolMaxHeaters))
	{
		return active ? controlToolActiveTarget[resource.heater] : controlToolStandbyTarget[resource.heater];
	}
	return 0;
}

static void RefreshControlToolsPage()
{
	if (controlToolHeaderCards[0] == nullptr)
	{
		return;
	}

	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour neutralBorder = UTFT::fromRGB(59, 67, 79);
	const Colour accent = UTFT::fromRGB(226, 69, 63);
	const Colour activePower = UTFT::fromRGB(192, 57, 47);
	const Colour activePowerGlyph = UTFT::fromRGB(58, 15, 12);
	const Colour inactivePower = UTFT::fromRGB(42, 49, 60);
	const Colour inactivePowerGlyph = UTFT::fromRGB(138, 146, 160);
	const Colour heaterFault = UTFT::fromRGB(128, 50, 205);

	const unsigned int resourceCount = CountControlToolResources();
	const unsigned int perPage = (resourceCount > ControlToolVisibleColumns) ? ControlToolPagedColumns : ControlToolVisibleColumns;
	const unsigned int maxPage = (resourceCount == 0) ? 0 : (resourceCount - 1) / perPage;
	if (controlToolPage > maxPage) controlToolPage = maxPage;

	for (unsigned int column = 0; column < ControlToolVisibleColumns; ++column)
	{
		const bool columnAllowed = column < perPage;
		const unsigned int resourcePos = controlToolPage * perPage + column;
		ControlToolResource resource;
		const bool visible = columnAllowed && resourcePos < resourceCount && GetControlToolResource(resourcePos, resource);
		controlToolVisibleResource[column] = visible ? resource : ControlToolResource{};
		mgr.Show(controlToolHeaderCards[column], visible);
		mgr.Show(controlToolNameFields[column], visible);
		mgr.Show(controlToolCurrentFields[column], visible);
		mgr.Show(controlToolActiveButtons[column], visible);
		mgr.Show(controlToolStandbyButtons[column], visible);
		mgr.Show(controlToolPowerButtons[column], visible);
		if (!visible) continue;

		OM::HeaterStatus state = GetControlToolHeaterStatus(resource);
		const bool active = state == OM::HeaterStatus::active;
		const bool standby = state == OM::HeaterStatus::standby;
		const bool fault = state == OM::HeaterStatus::fault ||
			(resource.heater >= 0 && resource.heater < static_cast<int>(JobStatusMaxHeaters) &&
			 jobStatusHeaterStatus[resource.heater] == OM::HeaterStatus::fault);

		if (resource.type == ControlToolResourceType::Tool)
		{
			controlToolNameText[column].printf("T%d", resource.index);
		}
		else if (resource.type == ControlToolResourceType::Bed)
		{
			controlToolNameText[column].copy("BED");
		}
		else
		{
			controlToolNameText[column].copy("CHAMBER");
		}
		controlToolNameFields[column]->SetText(controlToolNameText[column].c_str());
		controlToolNameFields[column]->SetIcon(resource.type == ControlToolResourceType::Bed ? ModernResourceIcon::Bed : ModernResourceIcon::None);

		if (resource.heater >= 0 && resource.heater < static_cast<int>(JobStatusMaxHeaters) && jobStatusHeaterValid[resource.heater])
		{
			controlToolCurrentText[column].printf("%.1f" DEGREE_SYMBOL "C", (double)jobStatusHeaterTemps[resource.heater]);
		}
		else
		{
			controlToolCurrentText[column].copy("---" DEGREE_SYMBOL "C");
		}
		controlToolCurrentFields[column]->SetValue(controlToolCurrentText[column].c_str());

		const int activeTarget = GetControlToolTarget(resource, true);
		const int standbyTarget = GetControlToolTarget(resource, false);
		controlToolActiveText[column].printf("%d", activeTarget);
		controlToolStandbyText[column].printf("%d", standbyTarget);
		controlToolActiveButtons[column]->SetText(controlToolActiveText[column].c_str());
		controlToolStandbyButtons[column]->SetText(controlToolStandbyText[column].c_str());
		controlToolActiveButtons[column]->SetEvent(evControlToolsActiveTemp, static_cast<int>(column));
		controlToolStandbyButtons[column]->SetEvent(evControlToolsStandbyTemp, static_cast<int>(column));

		const Colour headerFill = fault ? heaterFault : tile;
		const Colour headerText = fault ? text : (active ? accent : text);
		controlToolHeaderCards[column]->SetFillColour(headerFill);
		controlToolHeaderCards[column]->SetBorderVisible(active && !fault);
		controlToolHeaderCards[column]->SetBorderColour(accent);
		controlToolNameFields[column]->SetColours(headerText, headerFill);
		controlToolCurrentFields[column]->SetColours(headerText, headerFill);

		const Colour targetFill = fault ? heaterFault : tile;
		controlToolActiveButtons[column]->SetColours(fault ? text : (active ? accent : text), targetFill);
		controlToolActiveButtons[column]->SetBorderVisible(!fault && active);
		controlToolActiveButtons[column]->SetBorderColour(active ? accent : neutralBorder);
		controlToolStandbyButtons[column]->SetColours(fault ? text : (standby ? accent : text), targetFill);
		controlToolStandbyButtons[column]->SetBorderVisible(!fault && standby);
		controlToolStandbyButtons[column]->SetBorderColour(standby ? accent : neutralBorder);

		bool powered = false;
		bool allowPower = true;
		if (resource.type == ControlToolResourceType::Tool)
		{
			powered = currentTool == resource.index;
			const OM::PrinterStatus printerState = GetStatus();
			allowPower = printerState != OM::PrinterStatus::printing && printerState != OM::PrinterStatus::simulating;
		}
		else
		{
			powered = active;
		}
		controlToolPowerButtons[column]->SetColours(fault ? text : (powered ? activePowerGlyph : inactivePowerGlyph), fault ? heaterFault : (powered ? activePower : inactivePower));
		controlToolPowerButtons[column]->SetEvent(allowPower ? evControlToolsPower : evNull, static_cast<int>(column));
		controlToolPowerButtons[column]->SetBorderVisible(false);
		controlToolPowerButtons[column]->SetChanged();
	}

	const bool paged = resourceCount > ControlToolVisibleColumns;
	mgr.Show(controlToolPageUpButton, paged && controlToolPage > 0);
	mgr.Show(controlToolPageDownButton, paged && controlToolPage < maxPage);
}

static void RefreshControlTempNumpadValue()
{
	controlTempNumpadValueText.printf("%u", controlTempNumpadValue);
	if (controlTempNumpadValueField != nullptr)
	{
		controlTempNumpadValueField->SetValue(controlTempNumpadValueText.c_str());
	}
}

static void OpenControlTempNumpad(unsigned int column, bool activeTarget)
{
	if (column >= ControlToolVisibleColumns || controlToolVisibleResource[column].type == ControlToolResourceType::None)
	{
		return;
	}
	controlTempNumpadResource = controlToolVisibleResource[column];
	controlTempNumpadActiveTarget = activeTarget;
	int val = GetControlToolTarget(controlTempNumpadResource, activeTarget);
	if (val < 0) val = 0;
	if (val > 999) val = 999;
	controlTempNumpadValue = static_cast<unsigned int>(val);
	controlTempNumpadFresh = true;
	if (controlTempNumpadResource.type == ControlToolResourceType::Tool)
	{
		controlTempNumpadResourceText.printf("T%d", controlTempNumpadResource.index);
	}
	else if (controlTempNumpadResource.type == ControlToolResourceType::Bed)
	{
		controlTempNumpadResourceText.copy("BED");
	}
	else
	{
		controlTempNumpadResourceText.copy("CHAMBER");
	}
	controlTempNumpadResourceField->SetText(controlTempNumpadResourceText.c_str());
	RefreshControlTempNumpadValue();
	mgr.SetPopup(controlTempNumpadPopup, AutoPlace, AutoPlace);
}

static void SendControlTemperatureTarget()
{
	const int value = static_cast<int>(controlTempNumpadValue);
	const bool active = controlTempNumpadActiveTarget;
	const ControlToolResource resource = controlTempNumpadResource;
	if (resource.type == ControlToolResourceType::Tool)
	{
		OM::Tool * const tool = OM::GetTool(resource.index);
		if (tool == nullptr || tool->heaters[0] == nullptr) return;
		const bool useM568 = GetFirmwareFeatures().IsBitSet(m568TempAndRPM);
		if (nvData.GetHeaterCombineType() == HeaterCombineType::combined)
		{
			SerialIo::Sendf("%s P%d %c%d\n", useM568 ? "M568" : "G10", resource.index, active ? 'S' : 'R', value);
		}
		else
		{
			const int old = active ? tool->heaters[0]->activeTemp : tool->heaters[0]->standbyTemp;
			tool->UpdateTemp(0, value, active);
			String<maxUserCommandLength> temps;
			if (tool->GetHeaterTemps(temps.GetRef(), active))
			{
				SerialIo::Sendf("%s P%d %c%s\n", useM568 ? "M568" : "G10", resource.index, active ? 'S' : 'R', temps.c_str());
			}
			tool->UpdateTemp(0, old, active); // wait for RRF to authoritatively report the new target
		}
	}
	else if (resource.type == ControlToolResourceType::Bed)
	{
		SerialIo::Sendf("M140 P%d %c%d\n", resource.index, active ? 'S' : 'R', value);
	}
	else if (resource.type == ControlToolResourceType::Chamber)
	{
		SerialIo::Sendf("M141 P%d %c%d\n", resource.index, active ? 'S' : 'R', value);
	}
}

static void OpenControlToolChange(int targetTool)
{
	const OM::PrinterStatus printerState = GetStatus();
	if (printerState == OM::PrinterStatus::printing || printerState == OM::PrinterStatus::simulating)
	{
		return;
	}
	controlToolChangeTarget = (targetTool == currentTool) ? NoTool : targetTool;
	if (currentTool >= 0) controlToolChangeFromText.printf("T%d", currentTool);
	else controlToolChangeFromText.copy("OFF");
	if (controlToolChangeTarget >= 0) controlToolChangeToText.printf("T%d", controlToolChangeTarget);
	else controlToolChangeToText.copy("OFF");
	controlToolChangeFromField->SetText(controlToolChangeFromText.c_str());
	controlToolChangeToField->SetText(controlToolChangeToText.c_str());
	mgr.SetPopup(controlToolChangePopup, AutoPlace, AutoPlace);
}

static void HandleControlToolPower(unsigned int column)
{
	if (column >= ControlToolVisibleColumns) return;
	const ControlToolResource resource = controlToolVisibleResource[column];
	if (resource.type == ControlToolResourceType::Tool)
	{
		OpenControlToolChange(resource.index);
	}
	else if (resource.type == ControlToolResourceType::Bed)
	{
		const OM::Bed * const bed = OM::GetBed(resource.index);
		if (bed == nullptr) return;
		if (GetControlToolHeaterStatus(resource) == OM::HeaterStatus::active)
		{
			SerialIo::Sendf("M144 P%d\n", resource.index);
		}
		else
		{
			SerialIo::Sendf("M140 P%d S%d\n", resource.index, GetControlToolTarget(resource, true));
		}
	}
	else if (resource.type == ControlToolResourceType::Chamber)
	{
		const OM::Chamber * const chamber = OM::GetChamber(resource.index);
		if (chamber == nullptr) return;
		SerialIo::Sendf("M141 P%d S%d\n", resource.index,
			GetControlToolHeaterStatus(resource) == OM::HeaterStatus::active ? -274 : GetControlToolTarget(resource, true));
	}
}

static void CreateControlToolsPopups(const ColourScheme& colours)
{
	const Colour pageBg = UTFT::fromRGB(18, 22, 28);
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour neutralBorder = UTFT::fromRGB(59, 67, 79);
	const Colour accent = UTFT::fromRGB(226, 69, 63);
	const Colour cancelRed = UTFT::fromRGB(192, 57, 47);
	const Colour confirmGreen = UTFT::fromRGB(164, 214, 94);

	// Shared numeric temperature keypad, based on paneldue_numpad_popup_mockup_v9.svg.
	controlTempNumpadPopup = new PopupWindow(362, 450, pageBg, accent);
	DisplayField::SetDefaultFont(glcd19x21);
	DisplayField::SetDefaultColours(text, tile);
	controlTempNumpadValueField = new StaticTextField(35, 36, 185, TextAlignment::Left, "0");
	controlTempNumpadPopup->AddField(controlTempNumpadValueField);
	controlTempNumpadPopup->AddField(new StaticTextField(35, 245, 40, TextAlignment::Right, DEGREE_SYMBOL "C"));
	controlTempNumpadPopup->AddField(new ModernCard(20, 20, 280, 52, tile, neutralBorder, true));
	controlTempNumpadResourceField = new ModernTextButton(20, 310, 120, 52, "T0", evNull, 0, glcd19x21);
	controlTempNumpadPopup->AddField(controlTempNumpadResourceField);

	static const char * const digitLabels[10] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };
	for (unsigned int d = 1; d <= 9; ++d)
	{
		const unsigned int i = d - 1;
		const PixelNumber x = 20 + (i % 3) * 105;
		const PixelNumber y = 92 + (i / 3) * 64;
		controlTempNumpadPopup->AddField(new ModernTextButton(y, x, 95, 58, digitLabels[d], evNumericKey, static_cast<int>(d), glcd19x21));
	}
	controlTempNumpadPopup->AddField(new ModernTextButton(284, 125, 95, 58, digitLabels[0], evNumericKey, 0, glcd19x21));
	controlTempNumpadPopup->AddField(new ModernIconButton(92, 335, 95, 58, IconBackspace, evNumericBack));
	DisplayField::SetDefaultColours(pageBg, cancelRed);
	controlTempNumpadPopup->AddField(new ModernIconButton(156, 335, 95, 58, IconCancel, evNumericCancel));
	DisplayField::SetDefaultColours(pageBg, confirmGreen);
	controlTempNumpadPopup->AddField(new ModernIconButton(220, 335, 95, 58, IconOk, evNumericOk));

	// Tool-change confirmation popup, based on paneldue_tool_change_popup_mockup_v2.svg.
	controlToolChangePopup = new PopupWindow(450, 600, pageBg, accent);
	DisplayField::SetDefaultColours(text, pageBg);
	controlToolChangePopup->AddField(new StaticTextField(65, 30, 540, TextAlignment::Centre, "This operation requires tool"));
	controlToolChangePopup->AddField(new StaticTextField(105, 30, 540, TextAlignment::Centre, "change:"));
	DisplayField::SetDefaultColours(text, tile);
	controlToolChangeFromField = new ModernTextButton(180, 120, 140, 70, "OFF", evNull, 0, glcd19x21, true);
	controlToolChangeFromField->SetBorderColour(neutralBorder);
	controlToolChangeToField = new ModernTextButton(180, 340, 140, 70, "T0", evNull, 0, glcd19x21, true);
	controlToolChangeToField->SetBorderColour(neutralBorder);
	controlToolChangePopup->AddField(controlToolChangeFromField);
	controlToolChangePopup->AddField(controlToolChangeToField);
	DisplayField::SetDefaultColours(text, pageBg);
	controlToolChangePopup->AddField(new StaticTextField(202, 280, 40, TextAlignment::Centre, ">"));
	DisplayField::SetDefaultColours(pageBg, cancelRed);
	controlToolChangePopup->AddField(new ModernIconButton(320, 145, 140, 80, IconCancel, evControlToolChangeCancel));
	DisplayField::SetDefaultColours(pageBg, confirmGreen);
	controlToolChangePopup->AddField(new ModernIconButton(320, 315, 140, 80, IconOk, evControlToolChangeConfirm));
	DisplayField::SetDefaultFont(DEFAULT_FONT);
	UNUSED(colours);
}

static void CreateControlToolsTabFields(const ColourScheme& colours)
{
	mgr.SetRoot(baseRoot);
	const Colour pageBg = UTFT::fromRGB(18, 22, 28);
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour muted = UTFT::fromRGB(154, 164, 178);
	const Colour neutralBorder = UTFT::fromRGB(59, 67, 79);

	DisplayField::SetDefaultFont(glcd19x21);
	for (unsigned int column = 0; column < ControlToolVisibleColumns; ++column)
	{
		const PixelNumber x = ControlX(98 + column * 140);
		const PixelNumber w = ControlW(126);

		DisplayField::SetDefaultColours(text, tile);
		controlToolNameFields[column] = new ModernResourceLabel(100, x, w, 34, "", glcd19x21);
		controlToolCurrentFields[column] = new StaticTextField(153, x, w, TextAlignment::Centre, "---" DEGREE_SYMBOL "C");
		mgr.AddField(controlToolNameFields[column]);
		mgr.AddField(controlToolCurrentFields[column]);
		controlToolHeaderCards[column] = new ModernCard(88, x, w, 114, tile, neutralBorder, false);
		mgr.AddField(controlToolHeaderCards[column]);

		controlToolActiveText[column].copy("0");
		controlToolStandbyText[column].copy("0");
		controlToolActiveButtons[column] = new ModernTemperatureButton(210, x, w, 89, controlToolActiveText[column].c_str(),
			ModernTemperatureIcon::Active, evControlToolsActiveTemp, static_cast<int>(column), DEFAULT_FONT);
		controlToolStandbyButtons[column] = new ModernTemperatureButton(307, x, w, 89, controlToolStandbyText[column].c_str(),
			ModernTemperatureIcon::Standby, evControlToolsStandbyTemp, static_cast<int>(column), DEFAULT_FONT);
		mgr.AddField(controlToolActiveButtons[column]);
		mgr.AddField(controlToolStandbyButtons[column]);

		DisplayField::SetDefaultColours(muted, UTFT::fromRGB(42, 49, 60));
		controlToolPowerButtons[column] = new ModernPowerButton(404, x, w, 56, evControlToolsPower, static_cast<int>(column));
		mgr.AddField(controlToolPowerButtons[column]);
	}

	DisplayField::SetDefaultColours(text, tile);
	controlToolPageUpButton = new ModernIconButton(192, ControlX(736), ControlW(54), 118, IconUp, evControlToolsPageUp);
	controlToolPageDownButton = new ModernIconButton(316, ControlX(736), ControlW(54), 118, IconDown, evControlToolsPageDown);
	mgr.AddField(controlToolPageUpButton);
	mgr.AddField(controlToolPageDownButton);

	controlToolsRoot = mgr.GetRoot();
	// Add the four CONTROL sub-tabs to the modern TOOLS root.
	mgr.SetRoot(controlToolsRoot);
	AddTopTab(0, 4, "TOOLS", evControlTools);
	AddTopTab(1, 4, "MOVE", evControlMovement);
	AddTopTab(2, 4, "EXTRUDE", evControlExtrusion);
	AddTopTab(3, 4, "MACROS", evControlMacros);
	controlToolsRoot = mgr.GetRoot();

	// Background is intentionally added last so it is painted first by the linked-list renderer.
	mgr.SetRoot(controlToolsRoot);
	mgr.AddField(new ModernCard(0, masterTabWidth, DisplayX - masterTabWidth, DisplayY, pageBg, pageBg));
	controlToolsRoot = mgr.GetRoot();
	DisplayField::SetDefaultFont(DEFAULT_FONT);
	CreateControlToolsPopups(colours);
	RefreshControlToolsPage();
}

static OM::Axis *GetControlMoveAxis(unsigned int axisSlot)
{
	if (axisSlot >= ControlMoveAxisCount)
	{
		return nullptr;
	}
	const char wanted = "XYZ"[axisSlot];
	OM::Axis *result = nullptr;
	OM::IterateAxesWhile([&](OM::Axis*& axis, size_t) {
		if (axis != nullptr && axis->visible && toupper(axis->letter[0]) == wanted)
		{
			result = axis;
			return false;
		}
		return true;
	});
	return result;
}

static int GetControlMoveAxisSlot(char letter)
{
	switch (toupper(letter))
	{
	case 'X': return 0;
	case 'Y': return 1;
	case 'Z': return 2;
	default: return -1;
	}
}

static void RefreshControlMovePosition(unsigned int axisSlot)
{
	if (axisSlot >= ControlMoveAxisCount || controlMovePositionFields[axisSlot] == nullptr)
	{
		return;
	}
	if (!controlMovePositionValid[axisSlot])
	{
		controlMovePositionText[axisSlot].copy("---");
	}
	else
	{
		const float value = controlMovePosition[axisSlot];
		const int scaled100 = static_cast<int>(value * 100.0f + ((value >= 0.0f) ? 0.5f : -0.5f));
		const unsigned int magnitude = static_cast<unsigned int>((scaled100 < 0) ? -scaled100 : scaled100);
		if ((magnitude % 100u) == 0u)
		{
			controlMovePositionText[axisSlot].printf("%.0f", (double)value);
		}
		else if ((magnitude % 10u) == 0u)
		{
			controlMovePositionText[axisSlot].printf("%.1f", (double)value);
		}
		else
		{
			controlMovePositionText[axisSlot].printf("%.2f", (double)value);
		}
	}
	controlMovePositionFields[axisSlot]->SetValue(controlMovePositionText[axisSlot].c_str());
}

static void RefreshControlMoveSteps()
{
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour accent = UTFT::fromRGB(226, 69, 63);
	for (unsigned int i = 0; i < ControlMoveStepCount; ++i)
	{
		if (controlMoveStepButtons[i] == nullptr) continue;
		const bool selected = (i == controlMoveSelectedStep);
		controlMoveStepButtons[i]->SetColours(selected ? accent : text, tile);
		controlMoveStepButtons[i]->SetBorderVisible(selected);
		controlMoveStepButtons[i]->SetBorderColour(accent);
	}
}

static void RefreshControlMoveHoming()
{
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour normal = UTFT::fromRGB(188, 196, 207);
	const Colour accent = UTFT::fromRGB(226, 69, 63);
	for (unsigned int axisSlot = 0; axisSlot < ControlMoveAxisCount; ++axisSlot)
	{
		if (controlMoveHomeButtons[axisSlot] == nullptr) continue;
		const OM::Axis * const axis = GetControlMoveAxis(axisSlot);
		const bool exists = axis != nullptr;
		const bool homed = exists && axis->homed;
		controlMoveHomeButtons[axisSlot]->SetEvent(exists ? evControlMoveHome : evNull, static_cast<int>(axisSlot));
		controlMoveHomeButtons[axisSlot]->SetColours(homed ? accent : normal, tile);
		controlMoveHomeButtons[axisSlot]->SetBorderVisible(homed);
		controlMoveHomeButtons[axisSlot]->SetBorderColour(accent);
	}

	if (controlMoveHomeAllButton != nullptr)
	{
		bool anyVisible = false;
		bool allHomed = true;
		OM::IterateAxesWhile([&](OM::Axis*& axis, size_t) {
			if (axis != nullptr && axis->visible)
			{
				anyVisible = true;
				if (!axis->homed)
				{
					allHomed = false;
					return false;
				}
			}
			return true;
		});
		allHomed = anyVisible && allHomed;
		controlMoveHomeAllButton->SetColours(allHomed ? accent : normal, tile);
		controlMoveHomeAllButton->SetBorderVisible(allHomed);
		controlMoveHomeAllButton->SetBorderColour(accent);
	}
}

static void ShowModernAlert(const char *message)
{
	if (modernAlertPopup == nullptr || modernAlertMessageField == nullptr)
	{
		return;
	}
	modernAlertMessageText.copy(message != nullptr ? message : "ALERT");
	modernAlertMessageField->SetText(modernAlertMessageText.c_str());
	mgr.SetPopup(modernAlertPopup, AutoPlace, AutoPlace);
}

static void CreateModernAlertPopup()
{
	if (modernAlertPopup != nullptr)
	{
		return;
	}
	const Colour pageBg = UTFT::fromRGB(18, 22, 28);
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour neutralBorder = UTFT::fromRGB(59, 67, 79);
	const Colour accent = UTFT::fromRGB(226, 69, 63);
	const Colour cancelRed = UTFT::fromRGB(202, 57, 48);
	const Colour cancelGlyph = UTFT::fromRGB(74, 15, 12);

	modernAlertPopup = new PopupWindow(460, 610, pageBg, accent);
	DisplayField::SetDefaultFont(glcd19x21);
	DisplayField::SetDefaultColours(text, pageBg);
	modernAlertPopup->AddField(new StaticTextField(60, 0, 610, TextAlignment::Centre, "ALERT !"));
	DisplayField::SetDefaultColours(text, tile);
	modernAlertMessageText.copy("Printer AXIS not homed");
	modernAlertMessageField = new ModernTextButton(160, 105, 400, 80, modernAlertMessageText.c_str(), evNull, 0, DEFAULT_FONT, true);
	modernAlertMessageField->SetBorderColour(neutralBorder);
	modernAlertPopup->AddField(modernAlertMessageField);
	DisplayField::SetDefaultColours(cancelGlyph, cancelRed);
	modernAlertPopup->AddField(new ModernIconButton(300, 235, 140, 80, IconCancel, evModernAlertClose));
	DisplayField::SetDefaultFont(DEFAULT_FONT);
}

static void CreateControlMovementTabFields(const ColourScheme& colours)
{
	mgr.SetRoot(baseRoot);
	const Colour pageBg = UTFT::fromRGB(18, 22, 28);
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour muted = UTFT::fromRGB(154, 164, 178);
	const Colour neutralBorder = UTFT::fromRGB(59, 67, 79);
	DisplayField::SetDefaultFont(glcd19x21);

	// X/Y/Z position cards from the v7 SVG.
	const PixelNumber posSvgX[ControlMoveAxisCount] = { 118, 348, 578 };
	static const char * const axisLabels[ControlMoveAxisCount] = { "X:", "Y:", "Z:" };
	for (unsigned int axisSlot = 0; axisSlot < ControlMoveAxisCount; ++axisSlot)
	{
		const PixelNumber x = ControlX(posSvgX[axisSlot]);
		const PixelNumber w = ControlW(200);
		DisplayField::SetDefaultColours(muted, tile);
		mgr.AddField(new StaticTextField(97, x + ControlW(16), ControlW(44), TextAlignment::Left, axisLabels[axisSlot]));
		controlMovePositionText[axisSlot].copy("---");
		DisplayField::SetDefaultColours(text, tile);
		controlMovePositionFields[axisSlot] = new StaticTextField(97, x + ControlW(55), w - ControlW(69), TextAlignment::Right, controlMovePositionText[axisSlot].c_str());
		mgr.AddField(controlMovePositionFields[axisSlot]);
		mgr.AddField(new ModernCard(82, x, w, 52, tile, neutralBorder, true));
	}

	DisplayField::SetDefaultColours(muted, pageBg);
	mgr.AddField(new StaticTextField(151, ControlX(118), ControlW(220), TextAlignment::Left, "MOVE STEPS:"));

	// Step tiles: 0.1 / 0.02 on the first row, then 1 / 10 / 50.
	const PixelNumber stepX[ControlMoveStepCount] = { 118, 234, 118, 118, 118 };
	const PixelNumber stepY[ControlMoveStepCount] = { 190, 190, 260, 330, 400 };
	for (unsigned int i = 0; i < ControlMoveStepCount; ++i)
	{
		DisplayField::SetDefaultColours(text, tile);
		controlMoveStepButtons[i] = new ModernTextButton(stepY[i], ControlX(stepX[i]), ControlW(110), 62,
			controlMoveStepText[i], evControlMoveStep, static_cast<int>(i), glcd19x21, false);
		mgr.AddField(controlMoveStepButtons[i]);
	}

	// Jog buttons.
	struct JogButtonDef { PixelNumber x, y; const char *label; int param; };
	static const JogButtonDef jogButtons[] = {
		{ 400, 190, "Y+", 3 }, { 664, 190, "Z+", 5 },
		{ 268, 285, "X-", 0 }, { 532, 285, "X+", 1 },
		{ 400, 380, "Y-", 2 }, { 664, 380, "Z-", 4 }
	};
	for (const JogButtonDef& def : jogButtons)
	{
		DisplayField::SetDefaultColours(text, tile);
		mgr.AddField(new ModernTextButton(def.y, ControlX(def.x), ControlW(117), 80,
			def.label, evControlMoveJog, def.param, glcd19x21));
	}

	// Bed compensation (G32 -> bed.g).  It intentionally has no Accent state.
	DisplayField::SetDefaultColours(text, tile);
	controlMoveBedCompButton = new ModernBedCompButton(190, ControlX(532), ControlW(117), 80, evControlMoveBedComp);
	mgr.AddField(controlMoveBedCompButton);

	// Home ALL, X, Y, Z.  RRF homed state controls both the Accent outline and
	// the vector home glyph colour.
	DisplayField::SetDefaultColours(UTFT::fromRGB(188, 196, 207), tile);
	controlMoveHomeAllButton = new ModernHomeButton(285, ControlX(400), ControlW(117), 80, "ALL", evControlMoveHome, 3, DEFAULT_FONT);
	controlMoveHomeButtons[0] = new ModernHomeButton(380, ControlX(268), ControlW(117), 80, "X", evControlMoveHome, 0, DEFAULT_FONT);
	controlMoveHomeButtons[1] = new ModernHomeButton(380, ControlX(532), ControlW(117), 80, "Y", evControlMoveHome, 1, DEFAULT_FONT);
	controlMoveHomeButtons[2] = new ModernHomeButton(285, ControlX(664), ControlW(117), 80, "Z", evControlMoveHome, 2, DEFAULT_FONT);
	mgr.AddField(controlMoveHomeAllButton);
	mgr.AddField(controlMoveHomeButtons[0]);
	mgr.AddField(controlMoveHomeButtons[1]);
	mgr.AddField(controlMoveHomeButtons[2]);

	controlMovementRoot = mgr.GetRoot();
	mgr.SetRoot(controlMovementRoot);
	AddTopTab(0, 4, "TOOLS", evControlTools);
	AddTopTab(1, 4, "MOVE", evControlMovement);
	AddTopTab(2, 4, "EXTRUDE", evControlExtrusion);
	AddTopTab(3, 4, "MACROS", evControlMacros);
	controlMovementRoot = mgr.GetRoot();

	// Page background is added last because Window::AddField prepends fields.
	mgr.SetRoot(controlMovementRoot);
	mgr.AddField(new ModernCard(0, masterTabWidth, DisplayX - masterTabWidth, DisplayY, pageBg, pageBg));
	controlMovementRoot = mgr.GetRoot();
	DisplayField::SetDefaultFont(DEFAULT_FONT);
	RefreshControlMoveSteps();
	RefreshControlMoveHoming();
	for (unsigned int axisSlot = 0; axisSlot < ControlMoveAxisCount; ++axisSlot)
	{
		RefreshControlMovePosition(axisSlot);
	}
	CreateModernAlertPopup();
	UNUSED(colours);
}

static unsigned int CountControlExtrudeTools()
{
	unsigned int count = 0;
	OM::IterateToolsWhile([&count](OM::Tool*&, size_t) {
		++count;
		return true;
	});
	return count;
}

static OM::Tool *GetControlExtrudeToolByOrdinal(unsigned int ordinal)
{
	OM::Tool *result = nullptr;
	unsigned int pos = 0;
	OM::IterateToolsWhile([&](OM::Tool*& tool, size_t) {
		if (pos++ == ordinal)
		{
			result = tool;
			return false;
		}
		return true;
	});
	return result;
}

static void SelectControlExtrudePageForActiveTool()
{
	if (currentTool < 0)
	{
		return;
	}
	unsigned int ordinal = 0;
	bool found = false;
	OM::IterateToolsWhile([&](OM::Tool*& tool, size_t) {
		if (tool != nullptr && tool->index == currentTool)
		{
			found = true;
			return false;
		}
		++ordinal;
		return true;
	});
	if (found)
	{
		controlExtrudeToolPage = ordinal / ControlExtrudeToolsPerPage;
	}
}

static void RefreshControlExtrudeTools()
{
	if (controlExtrudeToolCards[0] == nullptr)
	{
		return;
	}
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour accent = UTFT::fromRGB(226, 69, 63);
	const Colour neutralBorder = UTFT::fromRGB(59, 67, 79);
	const unsigned int total = CountControlExtrudeTools();
	const unsigned int pageCount = (total + ControlExtrudeToolsPerPage - 1) / ControlExtrudeToolsPerPage;
	if (pageCount == 0)
	{
		controlExtrudeToolPage = 0;
	}
	else if (controlExtrudeToolPage >= pageCount)
	{
		controlExtrudeToolPage = pageCount - 1;
	}

	const unsigned int first = controlExtrudeToolPage * ControlExtrudeToolsPerPage;
	for (unsigned int row = 0; row < ControlExtrudeToolsPerPage; ++row)
	{
		OM::Tool * const tool = GetControlExtrudeToolByOrdinal(first + row);
		const bool visible = (tool != nullptr);
		mgr.Show(controlExtrudeToolCards[row], visible);
		mgr.Show(controlExtrudeToolNameFields[row], visible);
		mgr.Show(controlExtrudeToolTempFields[row], visible);
		if (!visible)
		{
			continue;
		}

		const bool active = (static_cast<int>(tool->index) == currentTool);
		controlExtrudeToolNameText[row].printf("T%d", tool->index);
		controlExtrudeToolNameFields[row]->SetValue(controlExtrudeToolNameText[row].c_str());
		controlExtrudeToolTempText[row].copy("---" DEGREE_SYMBOL "C");
		if (tool->heaters[0] != nullptr)
		{
			const unsigned int heater = tool->heaters[0]->heaterIndex;
			if (heater < JobStatusMaxHeaters && jobStatusHeaterValid[heater])
			{
				controlExtrudeToolTempText[row].printf("%.1f" DEGREE_SYMBOL "C", (double)jobStatusHeaterTemps[heater]);
			}
		}
		controlExtrudeToolTempFields[row]->SetValue(controlExtrudeToolTempText[row].c_str());
		controlExtrudeToolNameFields[row]->SetColours(active ? accent : text, tile);
		controlExtrudeToolTempFields[row]->SetColours(active ? accent : text, tile);
		controlExtrudeToolCards[row]->SetFillColour(tile);
		controlExtrudeToolCards[row]->SetBorderColour(active ? accent : neutralBorder);
		controlExtrudeToolCards[row]->SetBorderVisible(active);
	}

	if (controlExtrudePageUpButton != nullptr)
	{
		mgr.Show(controlExtrudePageUpButton, total > ControlExtrudeToolsPerPage && controlExtrudeToolPage > 0);
	}
	if (controlExtrudePageDownButton != nullptr)
	{
		mgr.Show(controlExtrudePageDownButton,
			total > ControlExtrudeToolsPerPage && first + ControlExtrudeToolsPerPage < total);
	}
}

static void RefreshControlExtrudeSelections()
{
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour accent = UTFT::fromRGB(226, 69, 63);
	for (unsigned int i = 0; i < ControlExtrudeSpeedCount; ++i)
	{
		if (controlExtrudeSpeedButtons[i] != nullptr)
		{
			const bool selected = (i == controlExtrudeSelectedSpeed);
			controlExtrudeSpeedButtons[i]->SetColours(selected ? accent : text, tile);
			controlExtrudeSpeedButtons[i]->SetBorderColour(accent);
			controlExtrudeSpeedButtons[i]->SetBorderVisible(selected);
		}
	}
	for (unsigned int i = 0; i < ControlExtrudeDistanceCount; ++i)
	{
		if (controlExtrudeDistanceButtons[i] != nullptr)
		{
			const bool selected = (i == controlExtrudeSelectedDistance);
			controlExtrudeDistanceButtons[i]->SetColours(selected ? accent : text, tile);
			controlExtrudeDistanceButtons[i]->SetBorderColour(accent);
			controlExtrudeDistanceButtons[i]->SetBorderVisible(selected);
		}
	}
}

static bool ControlExtrudeTemperatureReady(bool retract)
{
	if (currentTool < 0)
	{
		ShowModernAlert("No active tool");
		return false;
	}
	OM::Tool * const tool = OM::GetTool(static_cast<size_t>(currentTool));
	if (tool == nullptr || tool->extruders.IsEmpty())
	{
		ShowModernAlert("No active tool");
		return false;
	}

	// coldRetractTemperature was added after coldExtrudeTemperature.  If an
	// older RRF omits it, use the extrusion threshold as the safe fallback.
	const bool thresholdValid = retract
		? (controlColdRetractTemperatureValid || controlColdExtrudeTemperatureValid)
		: controlColdExtrudeTemperatureValid;
	const float threshold = (retract && controlColdRetractTemperatureValid)
		? controlColdRetractTemperature
		: controlColdExtrudeTemperature;
	if (!thresholdValid)
	{
		ShowModernAlert("Nozzle temperature too low");
		return false;
	}
	if (threshold <= 0.0f)
	{
		return true;
	}

	bool hasHeater = false;
	bool hotEnough = true;
	tool->IterateHeaters([&](OM::ToolHeater *heater, size_t) {
		if (heater == nullptr)
		{
			return;
		}
		hasHeater = true;
		const unsigned int heaterIndex = heater->heaterIndex;
		if (heaterIndex >= JobStatusMaxHeaters || !jobStatusHeaterValid[heaterIndex] ||
			jobStatusHeaterTemps[heaterIndex] < threshold)
		{
			hotEnough = false;
		}
	});
	if (!hasHeater || !hotEnough)
	{
		ShowModernAlert("Nozzle temperature too low");
		return false;
	}
	return true;
}

static void SendControlExtrudeAction(bool retract)
{
	if (!ControlExtrudeTemperatureReady(retract))
	{
		return;
	}
	SerialIo::Sendf("M120 M83 G1 E%s%s F%d M121\n",
		retract ? "-" : "",
		controlExtrudeDistanceParam[controlExtrudeSelectedDistance],
		controlExtrudeSpeedFeedrate[controlExtrudeSelectedSpeed]);
}

static void CreateControlExtrusionTabFields(const ColourScheme& colours)
{
	mgr.SetRoot(baseRoot);
	const Colour pageBg = UTFT::fromRGB(18, 22, 28);
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour actionTile = UTFT::fromRGB(42, 49, 60);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour muted = UTFT::fromRGB(154, 164, 178);
	const Colour neutralBorder = UTFT::fromRGB(59, 67, 79);
	DisplayField::SetDefaultFont(glcd19x21);

	DisplayField::SetDefaultColours(muted, pageBg);
	mgr.AddField(new StaticTextField(109, ControlX(118), ControlW(224), TextAlignment::Left, "Active tool:"));
	mgr.AddField(new StaticTextField(109, ControlX(362), ControlW(150), TextAlignment::Left, "Speed:"));

	// Tool rows are deliberately non-interactive information cards. The active
	// tool is indicated by Accent, but pressing a row never changes tools.
	for (unsigned int row = 0; row < ControlExtrudeToolsPerPage; ++row)
	{
		const PixelNumber y = 147 + row * 62;
		const PixelNumber x = ControlX(118);
		const PixelNumber w = ControlW(224);
		controlExtrudeToolNameText[row].printf("T%d", row);
		controlExtrudeToolTempText[row].copy("---" DEGREE_SYMBOL "C");
		DisplayField::SetDefaultColours(text, tile);
		controlExtrudeToolNameFields[row] = new StaticTextField(y + 16, x + ControlW(16), ControlW(70), TextAlignment::Left, controlExtrudeToolNameText[row].c_str());
		controlExtrudeToolTempFields[row] = new StaticTextField(y + 16, x + ControlW(88), w - ControlW(102), TextAlignment::Right, controlExtrudeToolTempText[row].c_str());
		mgr.AddField(controlExtrudeToolNameFields[row]);
		mgr.AddField(controlExtrudeToolTempFields[row]);
		controlExtrudeToolCards[row] = new ModernCard(y, x, w, 56, tile, neutralBorder, false);
		mgr.AddField(controlExtrudeToolCards[row]);
	}

	for (unsigned int i = 0; i < ControlExtrudeSpeedCount; ++i)
	{
		DisplayField::SetDefaultColours(text, tile);
		controlExtrudeSpeedButtons[i] = new ModernTextButton(147 + i * 62, ControlX(362), ControlW(150), 56,
			controlExtrudeSpeedText[i], evControlExtrudeSpeed, static_cast<int>(i), glcd19x21);
		mgr.AddField(controlExtrudeSpeedButtons[i]);
	}

	for (unsigned int i = 0; i < ControlExtrudeDistanceCount; ++i)
	{
		DisplayField::SetDefaultColours(text, tile);
		controlExtrudeDistanceButtons[i] = new ModernTextButton(147 + i * 62, ControlX(532), ControlW(150), 56,
			controlExtrudeDistanceText[i], evControlExtrudeDistance, static_cast<int>(i), glcd19x21);
		mgr.AddField(controlExtrudeDistanceButtons[i]);
	}

	DisplayField::SetDefaultColours(text, actionTile);
	controlExtrudeRetractButton = new ModernTextButton(83, ControlX(532), ControlW(150), 60,
		"RETRACT", evControlExtrudeAction, -1, glcd19x21);
	controlExtrudeExtrudeButton = new ModernTextButton(393, ControlX(532), ControlW(150), 60,
		"EXTRUDE", evControlExtrudeAction, 1, glcd19x21);
	mgr.AddField(controlExtrudeRetractButton);
	mgr.AddField(controlExtrudeExtrudeButton);

	DisplayField::SetDefaultColours(text, tile);
	controlExtrudePageUpButton = new ModernIconButton(147, ControlX(736), ControlW(54), 118, IconUp, evControlExtrudePageUp);
	controlExtrudePageDownButton = new ModernIconButton(271, ControlX(736), ControlW(54), 118, IconDown, evControlExtrudePageDown);
	mgr.AddField(controlExtrudePageUpButton);
	mgr.AddField(controlExtrudePageDownButton);

	controlExtrusionRoot = mgr.GetRoot();
	mgr.SetRoot(controlExtrusionRoot);
	AddTopTab(0, 4, "TOOLS", evControlTools);
	AddTopTab(1, 4, "MOVE", evControlMovement);
	AddTopTab(2, 4, "EXTRUDE", evControlExtrusion);
	AddTopTab(3, 4, "MACROS", evControlMacros);
	controlExtrusionRoot = mgr.GetRoot();

	mgr.SetRoot(controlExtrusionRoot);
	mgr.AddField(new ModernCard(0, masterTabWidth, DisplayX - masterTabWidth, DisplayY, pageBg, pageBg));
	controlExtrusionRoot = mgr.GetRoot();
	DisplayField::SetDefaultFont(DEFAULT_FONT);
	SelectControlExtrudePageForActiveTool();
	RefreshControlExtrudeTools();
	RefreshControlExtrudeSelections();
	UNUSED(colours);
}

static PixelNumber TuneX(PixelNumber svgX)
{
	return contentLeft + static_cast<PixelNumber>((static_cast<uint32_t>(svgX - 90) * contentWidth) / 710);
}

static PixelNumber TuneW(PixelNumber svgW)
{
	const PixelNumber w = static_cast<PixelNumber>((static_cast<uint32_t>(svgW) * contentWidth) / 710);
	return (w == 0) ? 1 : w;
}

static PixelNumber JobX(PixelNumber svgX)
{
	return contentLeft + static_cast<PixelNumber>((static_cast<uint32_t>(svgX - 90) * contentWidth) / 710);
}

static PixelNumber JobW(PixelNumber svgW)
{
	const PixelNumber w = static_cast<PixelNumber>((static_cast<uint32_t>(svgW) * contentWidth) / 710);
	return (w == 0) ? 1 : w;
}

static PixelNumber ObjectX(PixelNumber svgX)
{
	return contentLeft + static_cast<PixelNumber>((static_cast<uint32_t>(svgX - 90) * contentWidth) / 710);
}

static PixelNumber ObjectW(PixelNumber svgW)
{
	const PixelNumber w = static_cast<PixelNumber>((static_cast<uint32_t>(svgW) * contentWidth) / 710);
	return (w == 0) ? 1 : w;
}

static int FindNamedFan(const char *name)
{
	for (unsigned int i = 0; i < TuneMaxFans; ++i)
	{
		if (tuneFanValid[i] && strcasecmp(tuneFanNames[i].c_str(), name) == 0)
		{
			return static_cast<int>(i);
		}
	}
	return -1;
}

static int GetJobStatusToolExtruder()
{
	if (currentTool < 0)
	{
		return -1;
	}
	OM::Tool * const tool = OM::GetTool(static_cast<size_t>(currentTool));
	return (tool != nullptr && !tool->extruders.IsEmpty()) ? static_cast<int>(tool->extruders.LowestSetBit()) : -1;
}

static int GetJobStatusToolFan()
{
	if (currentTool < 0)
	{
		return -1;
	}
	OM::Tool * const tool = OM::GetTool(static_cast<size_t>(currentTool));
	return (tool != nullptr && !tool->fans.IsEmpty()) ? static_cast<int>(tool->fans.LowestSetBit()) : -1;
}

static int GetJobStatusToolHeater()
{
	if (currentTool < 0)
	{
		return -1;
	}
	OM::Tool * const tool = OM::GetTool(static_cast<size_t>(currentTool));
	if (tool == nullptr || tool->heaters[0] == nullptr)
	{
		return -1;
	}
	return static_cast<int>(tool->heaters[0]->heaterIndex);
}

static int GetJobStatusBedHeater()
{
	OM::Bed * const bed = OM::GetFirstBed();
	return (bed != nullptr) ? static_cast<int>(bed->heater) : -1;
}

static int GetJobStatusChamberHeater()
{
	OM::Chamber * const chamber = OM::GetFirstChamber();
	return (chamber != nullptr) ? static_cast<int>(chamber->heater) : -1;
}

static const char *JobStatusTileLabel(JobStatusTileType type, String<20>& label)
{
	switch (type)
	{
	case JobStatusTileType::ToolTemp:
		label.printf((currentTool >= 0) ? "T%d" : "TOOL TEMP", currentTool);
		break;
	case JobStatusTileType::BedTemp: label.copy("BED"); break;
	case JobStatusTileType::ChamberTemp: label.copy("CHAMBER"); break;
	case JobStatusTileType::FanPart:
		label.printf((currentTool >= 0) ? "FAN T%d" : "FAN PART", currentTool);
		break;
	case JobStatusTileType::FanAux: label.copy("FAN AUX"); break;
	case JobStatusTileType::FanCha: label.copy("FAN ->I->"); break;
	case JobStatusTileType::SpeedReq: label.copy("SPEED REQ:"); break;
	case JobStatusTileType::SpeedCur: label.copy("SPEED CU:"); break;
	case JobStatusTileType::FlowFactor: label.copy("FLOW FAC:"); break;
	case JobStatusTileType::FlowVol: label.copy("FLOW VOL:"); break;
	}
	return label.c_str();
}

static void RefreshJobStatusTile(unsigned int slot)
{
	if (slot >= JobStatusTileCount || jobStatusLabels[slot] == nullptr || jobStatusValues[slot] == nullptr)
	{
		return;
	}
	const JobStatusTileType type = jobStatusTiles[slot];
	jobStatusLabels[slot]->SetValue(JobStatusTileLabel(type, jobStatusLabelText[slot]));
	String<24>& value = jobStatusValueText[slot];
	value.copy("---");

	int index = -1;
	int heaterIndex = -1;
	switch (type)
	{
	case JobStatusTileType::ToolTemp:
		index = GetJobStatusToolHeater();
		heaterIndex = index;
		if (index >= 0 && index < static_cast<int>(JobStatusMaxHeaters) && jobStatusHeaterValid[index])
		{
			value.printf("%.1f" DEGREE_SYMBOL "C", (double)jobStatusHeaterTemps[index]);
		}
		break;
	case JobStatusTileType::BedTemp:
		index = GetJobStatusBedHeater();
		heaterIndex = index;
		if (index >= 0 && index < static_cast<int>(JobStatusMaxHeaters) && jobStatusHeaterValid[index])
		{
			value.printf("%.1f" DEGREE_SYMBOL "C", (double)jobStatusHeaterTemps[index]);
		}
		break;
	case JobStatusTileType::ChamberTemp:
		index = GetJobStatusChamberHeater();
		heaterIndex = index;
		if (index >= 0 && index < static_cast<int>(JobStatusMaxHeaters) && jobStatusHeaterValid[index])
		{
			value.printf("%.1f" DEGREE_SYMBOL "C", (double)jobStatusHeaterTemps[index]);
		}
		break;
	case JobStatusTileType::FanPart:
		index = GetJobStatusToolFan();
		if (index >= 0 && index < static_cast<int>(TuneMaxFans) && tuneFanValid[index])
		{
			value.printf("%d%%", tuneFanPercent[index]);
		}
		break;
	case JobStatusTileType::FanAux:
		index = FindNamedFan("FAN_AUX");
		if (index >= 0) value.printf("%d%%", tuneFanPercent[index]);
		break;
	case JobStatusTileType::FanCha:
		index = FindNamedFan("FAN_CHA");
		if (index >= 0) value.printf("%d%%", tuneFanPercent[index]);
		break;
	case JobStatusTileType::SpeedReq:
		value.printf("%.0f mm/s", (double)jobStatusRequestedSpeed);
		break;
	case JobStatusTileType::SpeedCur:
		value.printf("%.0f mm/s", (double)jobStatusTopSpeed);
		break;
	case JobStatusTileType::FlowFactor:
		index = GetJobStatusToolExtruder();
		if (index >= 0 && index < static_cast<int>(TuneMaxExtruders))
		{
			value.printf("%d%%", tuneExtruderFactor[index]);
		}
		break;
	case JobStatusTileType::FlowVol:
		index = GetJobStatusToolExtruder();
		if (index >= 0 && index < static_cast<int>(JobStatusMaxExtruders) && jobStatusFilamentDiameterValid[index])
		{
			const float d = jobStatusFilamentDiameter[index];
			const float area = 0.7853981634f * d * d;
			value.printf("%.1f mm3/s", (double)(jobStatusExtrusionRate * area));
		}
		break;
	}

	const Colour normalTile = UTFT::fromRGB(28, 34, 43);
	const Colour muted = UTFT::fromRGB(154, 164, 178);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour heaterFault = UTFT::fromRGB(128, 50, 205);
	const bool fault = heaterIndex >= 0 && heaterIndex < static_cast<int>(JobStatusMaxHeaters) &&
		jobStatusHeaterStatus[heaterIndex] == OM::HeaterStatus::fault;
	const Colour cardColour = fault ? heaterFault : normalTile;
	jobStatusCards[slot]->SetFillColour(cardColour);
	jobStatusLabels[slot]->SetColours(muted, cardColour);
	jobStatusValues[slot]->SetColours(text, cardColour);
	jobStatusValues[slot]->SetValue(value.c_str());
}

static void RefreshJobStatusTiles()
{
	for (unsigned int i = 0; i < JobStatusTileCount; ++i)
	{
		RefreshJobStatusTile(i);
	}
}

static void RefreshJobStatusTilesByType(JobStatusTileType type)
{
	for (unsigned int i = 0; i < JobStatusTileCount; ++i)
	{
		if (jobStatusTiles[i] == type)
		{
			RefreshJobStatusTile(i);
		}
	}
}

static void RefreshJobStatusHeader()
{
	if (jobStatusNameField != nullptr)
	{
		jobStatusNameText.copy(printingFile.IsEmpty() ? "No active print" : printingFile.c_str());
		jobStatusNameField->SetValue(jobStatusNameText.c_str());
	}
	if (jobStatusProgressField != nullptr)
	{
		jobStatusProgressText.printf("%u%%", constrain<unsigned int>(jobStatusProgress, 0, 100));
		jobStatusProgressField->SetValue(jobStatusProgressText.c_str());
	}
	if (jobStatusLayersField != nullptr)
	{
		if (jobStatusNumLayers != 0)
		{
			jobStatusLayersText.printf("%u / %u", jobStatusLayer, jobStatusNumLayers);
		}
		else
		{
			jobStatusLayersText.printf("%u / ---", jobStatusLayer);
		}
		jobStatusLayersField->SetValue(jobStatusLayersText.c_str());
	}
	if (jobStatusTimeField != nullptr)
	{
		const uint32_t hours = jobStatusDuration / 3600;
		const uint32_t mins = (jobStatusDuration / 60) % 60;
		const uint32_t secs = jobStatusDuration % 60;
		jobStatusTimeText.printf("%lu : %02lu : %02lu", (unsigned long)hours, (unsigned long)mins, (unsigned long)secs);
		jobStatusTimeField->SetValue(jobStatusTimeText.c_str());
	}
}

static void RefreshJobStatusActions()
{
	if (jobStatusPauseResumeButton == nullptr)
	{
		return;
	}
	const OM::PrinterStatus stat = GetStatus();
	const bool paused = (stat == OM::PrinterStatus::paused || stat == OM::PrinterStatus::resuming);
	jobStatusPauseResumeButton->SetText(paused ? "> RESUME" : "|| PAUSE");
	const bool canPauseResume = (stat == OM::PrinterStatus::printing || stat == OM::PrinterStatus::paused ||
		stat == OM::PrinterStatus::pausing || stat == OM::PrinterStatus::resuming);
	mgr.Show(jobStatusPauseResumeButton, canPauseResume);
	mgr.Show(jobStatusAbortButton, canPauseResume || stat == OM::PrinterStatus::simulating);
}

static void JobStatusThumbnailRefreshNotify(bool full, bool changed)
{
	if ((!full && !changed) || printingFile.IsEmpty() || !PrintInProgress())
	{
		return;
	}
	SerialIo::Sendf(GetFirmwareFeatures().IsBitSet(noM20M36) ? "M408 S36 P\"%s\"\n" : "M36 \"%s\"\n", printingFile.c_str());
}

static void OpenJobStatusConfirmation(JobStatusConfirmAction action)
{
	jobStatusConfirmAction = action;
	switch (action)
	{
	case JobStatusConfirmAction::Pause: jobStatusConfirmText.copy("PAUSE PRINT?"); break;
	case JobStatusConfirmAction::Resume: jobStatusConfirmText.copy("RESUME PRINT?"); break;
	case JobStatusConfirmAction::Abort: jobStatusConfirmText.copy("ABORT PRINT?"); break;
	default: jobStatusConfirmText.copy("CONFIRM?"); break;
	}
	jobStatusConfirmTitle->SetText(jobStatusConfirmText.c_str());
	mgr.SetPopup(jobStatusConfirmPopup, AutoPlace, AutoPlace);
}

static void CreateStatusJobStatusConfirmPopup(const ColourScheme& colours)
{
	const Colour pageBg = UTFT::fromRGB(18, 22, 28);
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour cancelRed = UTFT::fromRGB(192, 57, 47);
	const Colour confirmGreen = UTFT::fromRGB(164, 214, 94);
	jobStatusConfirmPopup = new PopupWindow(360, 600, pageBg, colours.popupBorderColour);
	DisplayField::SetDefaultFont(glcd19x21);
	DisplayField::SetDefaultColours(text, tile);
	jobStatusConfirmTitle = new ModernTextButton(70, 30, 540, 80, "CONFIRM?", evNull, 0, glcd19x21);
	jobStatusConfirmPopup->AddField(jobStatusConfirmTitle);
	DisplayField::SetDefaultColours(text, cancelRed);
	jobStatusConfirmPopup->AddField(new ModernIconButton(230, 145, 140, 80, IconCancel, evStatusJobStatusCancel));
	DisplayField::SetDefaultColours(text, confirmGreen);
	jobStatusConfirmPopup->AddField(new ModernIconButton(230, 315, 140, 80, IconOk, evStatusJobStatusConfirm));
	DisplayField::SetDefaultFont(DEFAULT_FONT);
}

static void CreateStatusJobStatusTabFields(const ColourScheme& colours)
{
	mgr.SetRoot(baseRoot);
	const Colour pageBg = UTFT::fromRGB(18, 22, 28);
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour muted = UTFT::fromRGB(154, 164, 178);
	const Colour pauseCyan = UTFT::fromRGB(95, 195, 220);
	const Colour pauseText = UTFT::fromRGB(11, 31, 36);
	const Colour abortRed = UTFT::fromRGB(192, 57, 47);
	const Colour abortText = UTFT::fromRGB(58, 15, 12);

	DisplayField::SetDefaultFont(glcd19x21);

	// Job name and compact file-progress tile. Text is registered before the card because
	// Window::AddField prepends fields, so the card is painted first and the text on top.
	DisplayField::SetDefaultColours(text, tile);
	jobStatusNameField = new StaticTextField(91, JobX(132), JobW(544), TextAlignment::Left, "No active print");
	mgr.AddField(jobStatusNameField);
	jobStatusNameCard = new ModernCard(80, JobX(118), JobW(572), 44, tile, tile);
	mgr.AddField(jobStatusNameCard);
	jobStatusProgressField = new StaticTextField(91, JobX(700), JobW(80), TextAlignment::Centre, "0%");
	mgr.AddField(jobStatusProgressField);
	jobStatusProgressCard = new ModernCard(80, JobX(700), JobW(80), 44, tile, tile);
	mgr.AddField(jobStatusProgressCard);

	// Thumbnail card and direct-draw target. QOI pixels are converted to RGB565 as they arrive.
	jobStatusThumbnail = new DrawDirect(151, JobX(118), 220, 220, JobStatusThumbnailRefreshNotify);
	mgr.AddField(jobStatusThumbnail);
	jobStatusThumbnailCard = new ModernCard(151, JobX(118), 220, 220, tile, tile);
	mgr.AddField(jobStatusThumbnailCard);

	// 3x3 read-only information grid.
	for (unsigned int i = 0; i < JobStatusTileCount; ++i)
	{
		const unsigned int row = i / 3;
		const unsigned int col = i % 3;
		const PixelNumber x = JobX(358 + col * 146);
		const PixelNumber y = 151 + row * 77;
		const PixelNumber w = JobW(130);

		DisplayField::SetDefaultColours(muted, tile);
		jobStatusLabels[i] = new StaticTextField(y + 10, x, w, TextAlignment::Centre, "");
		mgr.AddField(jobStatusLabels[i]);
		DisplayField::SetDefaultColours(text, tile);
		jobStatusValues[i] = new StaticTextField(y + 37, x, w, TextAlignment::Centre, "---");
		mgr.AddField(jobStatusValues[i]);
		jobStatusCards[i] = new ModernCard(y, x, w, 67, tile, tile);
		mgr.AddField(jobStatusCards[i]);
	}

	// Bottom information fields.
	DisplayField::SetDefaultColours(text, tile);
	jobStatusLayersField = new StaticTextField(421, JobX(118), JobW(150), TextAlignment::Centre, "0 / ---");
	mgr.AddField(jobStatusLayersField);
	mgr.AddField(new ModernCard(407, JobX(118), JobW(150), 50, tile, tile));
	jobStatusTimeField = new StaticTextField(421, JobX(288), JobW(150), TextAlignment::Centre, "0 : 00 : 00");
	mgr.AddField(jobStatusTimeField);
	mgr.AddField(new ModernCard(407, JobX(288), JobW(150), 50, tile, tile));
	DisplayField::SetDefaultColours(muted, pageBg);
	mgr.AddField(new StaticTextField(387, JobX(118), JobW(150), TextAlignment::Left, "Layers:"));
	mgr.AddField(new StaticTextField(387, JobX(288), JobW(150), TextAlignment::Left, "Time:"));

	DisplayField::SetDefaultColours(pauseText, pauseCyan);
	jobStatusPauseResumeButton = new ModernTextButton(407, JobX(490), JobW(140), 50, "|| PAUSE", evStatusJobStatusPauseResume, 0, glcd19x21);
	mgr.AddField(jobStatusPauseResumeButton);
	DisplayField::SetDefaultColours(abortText, abortRed);
	jobStatusAbortButton = new ModernTextButton(407, JobX(640), JobW(140), 50, "ABORT!", evStatusJobStatusAbort, 0, glcd19x21);
	mgr.AddField(jobStatusAbortButton);

	statusJobStatusRoot = mgr.GetRoot();
	AddStatusSubTabs(statusJobStatusRoot);
	mgr.SetRoot(statusJobStatusRoot);
	mgr.AddField(new ModernCard(0, masterTabWidth, DisplayX - masterTabWidth, DisplayY, pageBg, pageBg));
	statusJobStatusRoot = mgr.GetRoot();
	DisplayField::SetDefaultFont(DEFAULT_FONT);
	CreateStatusJobStatusConfirmPopup(colours);
	RefreshJobStatusTiles();
	RefreshJobStatusHeader();
	RefreshJobStatusActions();
}

static void CreateStatusJobStartPopup(const ColourScheme& colours)
{
	const Colour pageBg = UTFT::fromRGB(18, 22, 28);
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour neutralBorder = UTFT::fromRGB(59, 67, 79);
	const Colour cancelRed = UTFT::fromRGB(192, 57, 47);
	const Colour confirmGreen = UTFT::fromRGB(164, 214, 94);

	// SVG reference is a 600x450 popup (inside a 610x460 drawing canvas).
	statusJobStartPopup = new PopupWindow(450, 600, pageBg, colours.popupBorderColour);
	DisplayField::SetDefaultFont(glcd19x21);
	DisplayField::SetDefaultColours(text, pageBg);
	statusJobStartPopup->AddField(new StaticTextField(70, 30, 540, TextAlignment::Centre, "Start print JOB:"));

	DisplayField::SetDefaultColours(text, tile);
	statusJobStartFileField = new ModernTextButton(152, 30, 540, 70, "", evNull, 0, glcd19x21, true);
	statusJobStartFileField->SetBorderColour(neutralBorder);
	statusJobStartPopup->AddField(statusJobStartFileField);

	DisplayField::SetDefaultColours(text, cancelRed);
	statusJobStartPopup->AddField(new ModernIconButton(300, 145, 140, 80, IconCancel, evStatusJobPrintCancel));
	DisplayField::SetDefaultColours(text, confirmGreen);
	statusJobStartPopup->AddField(new ModernIconButton(300, 315, 140, 80, IconOk, evStatusJobPrintConfirm));
	DisplayField::SetDefaultFont(DEFAULT_FONT);
}

static void CreateStatusJobTabFields(const ColourScheme& colours)
{
	mgr.SetRoot(baseRoot);
	const Colour pageBg = UTFT::fromRGB(18, 22, 28);
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);

	DisplayField::SetDefaultFont(glcd19x21);
	DisplayField::SetDefaultColours(text, tile);
	for (unsigned int row = 0; row < StatusJobRows; ++row)
	{
		const PixelNumber y = 85 + row * 62;
		statusJobFileButtons[row] = new ModernTextButton(y, JobX(118), JobW(598), 56, nullptr, evNull, 0, glcd19x21, false, TextAlignment::Left);
		mgr.AddField(statusJobFileButtons[row]);
		mgr.Show(statusJobFileButtons[row], false);
	}

	statusJobPageUpButton = new ModernIconButton(147, JobX(736), JobW(54), 118, IconUp, evStatusJobPageUp);
	statusJobPageDownButton = new ModernIconButton(271, JobX(736), JobW(54), 118, IconDown, evStatusJobPageDown);
	mgr.AddField(statusJobPageUpButton);
	mgr.AddField(statusJobPageDownButton);
	mgr.Show(statusJobPageUpButton, false);
	mgr.Show(statusJobPageDownButton, false);

	statusJobRoot = mgr.GetRoot();
	AddStatusSubTabs(statusJobRoot);

	// Paint the content area behind the JOB fields, matching the modern STATUS pages.
	mgr.SetRoot(statusJobRoot);
	DisplayField::SetDefaultColours(text, pageBg);
	mgr.AddField(new ModernTextButton(0, masterTabWidth, DisplayX - masterTabWidth, DisplayY, nullptr, evNull, 0, glcd19x21));
	statusJobRoot = mgr.GetRoot();
	DisplayField::SetDefaultFont(DEFAULT_FONT);

	CreateStatusJobStartPopup(colours);
}

static void UpdateTunePopupValue()
{
	if (tunePopupKind == TunePopupKind::PressureAdvance)
	{
		tunePopupValueText.printf("%.4f", (double)tunePopupPa);
		if (tunePressureAdvancePopupValue != nullptr)
		{
			tunePressureAdvancePopupValue->SetText(tunePopupValueText.c_str());
		}
	}
	else
	{
		if (tunePopupKind == TunePopupKind::Speed)
		{
			tunePopupValueText.printf("%d %%", tunePopupPercent);
		}
		else
		{
			tunePopupValueText.printf("%d%%", tunePopupPercent);
		}
		ModernTextButton *value = nullptr;
		if (tunePopupKind == TunePopupKind::Fan)
		{
			value = tuneFanPopupValue;
		}
		else if (tunePopupKind == TunePopupKind::Speed)
		{
			value = tuneSpeedPopupValue;
		}
		else
		{
			value = tuneFeedRatePopupValue;
		}
		if (value != nullptr)
		{
			value->SetText(tunePopupValueText.c_str());
		}
	}
}

static void OpenTunePressureAdvancePopup(int toolIndex, int extruder)
{
	if (extruder < 0 || extruder >= (int)TuneMaxExtruders)
	{
		return;
	}
	tunePopupKind = TunePopupKind::PressureAdvance;
	tunePopupResource = extruder;
	tunePopupPa = tunePressureAdvance[extruder];
	tunePopupTitleText.printf("PRESSURE ADVANCE T%d", toolIndex);
	tunePressureAdvancePopupTitle->SetText(tunePopupTitleText.c_str());
	UpdateTunePopupValue();
	mgr.SetPopup(tunePressureAdvancePopup, AutoPlace, AutoPlace);
}

static void OpenTuneFanPopup(const char *title, int toolIndex, int fanIndex)
{
	UNUSED(toolIndex);
	if (fanIndex < 0 || fanIndex >= (int)TuneMaxFans)
	{
		return;
	}
	tunePopupKind = TunePopupKind::Fan;
	tunePopupResource = fanIndex;
	tunePopupPercent = tuneFanPercent[fanIndex];
	tunePopupTitleText.copy(title);
	tuneFanPopupTitle->SetText(tunePopupTitleText.c_str());
	UpdateTunePopupValue();
	mgr.SetPopup(tuneFanPopup, AutoPlace, AutoPlace);
}

static void OpenTuneSpeedPopup()
{
	tunePopupKind = TunePopupKind::Speed;
	tunePopupResource = -1;
	tunePopupPercent = tuneSpeedPercent;
	UpdateTunePopupValue();
	mgr.SetPopup(tuneSpeedPopup, AutoPlace, AutoPlace);
}

static void OpenTuneFeedPopup(int toolIndex, int extruder)
{
	tunePopupKind = TunePopupKind::Flow;
	tunePopupResource = extruder;
	tunePopupPercent = (extruder >= 0 && extruder < (int)TuneMaxExtruders) ? tuneExtruderFactor[extruder] : 100;
	tunePopupTitleText.printf("FEED RATE T%d", toolIndex);
	tuneFeedRatePopupTitle->SetText(tunePopupTitleText.c_str());
	UpdateTunePopupValue();
	mgr.SetPopup(tuneFeedRatePopup, AutoPlace, AutoPlace);
}

static void RefreshTuneGeneralFans()
{
	bool toolFan[TuneMaxFans] = { false };
	OM::IterateToolsWhile([&toolFan](OM::Tool*& tool, size_t) {
		for (unsigned int fan = 0; fan < TuneMaxFans; ++fan)
		{
			if (tool->fans.IsBitSet(fan))
			{
				toolFan[fan] = true;
			}
		}
		return true;
	});

	// General TUNE fans are selected by their RRF fan names, not by fan number.
	// FAN_AUX is the auxiliary fan and FAN_CHA is the chamber/filter fan.
	tuneGeneralFanIndices[0] = -1;
	tuneGeneralFanIndices[1] = -1;
	for (unsigned int fan = 0; fan < TuneMaxFans; ++fan)
	{
		if (!tuneFanValid[fan] || toolFan[fan])
		{
			continue;
		}

		if (strcmp(tuneFanNames[fan].c_str(), "FAN_AUX") == 0)
		{
			tuneGeneralFanIndices[0] = (int)fan;
		}
		else if (strcmp(tuneFanNames[fan].c_str(), "FAN_CHA") == 0)
		{
			tuneGeneralFanIndices[1] = (int)fan;
		}
	}
	for (unsigned int i = 0; i < 2; ++i)
	{
		const int fan = tuneGeneralFanIndices[i];
		if (fan >= 0)
		{
			tuneGeneralFanText[i].printf("%d%%", tuneFanPercent[fan]);
			tuneGeneralFanButtons[i]->SetText(tuneGeneralFanText[i].c_str());
			mgr.Show(tuneGeneralFanButtons[i], true);
			mgr.Show(tuneGeneralFanLabels[i], true);
		}
		else
		{
			mgr.Show(tuneGeneralFanButtons[i], false);
			mgr.Show(tuneGeneralFanLabels[i], false);
		}
	}
}

static void RefreshTuneToolRows()
{
	OM::Tool *tools[MaxSlots] = { nullptr };
	unsigned int toolCount = 0;
	OM::IterateToolsWhile([&tools, &toolCount](OM::Tool*& tool, size_t) {
		if (toolCount < MaxSlots)
		{
			tools[toolCount++] = tool;
		}
		return toolCount < MaxSlots;
	});

	const unsigned int maxPage = (toolCount == 0) ? 0 : (toolCount - 1) / TuneToolsPerPage;
	if (tuneToolPage > maxPage)
	{
		tuneToolPage = maxPage;
	}

	for (unsigned int row = 0; row < TuneToolsPerPage; ++row)
	{
		const unsigned int toolPos = tuneToolPage * TuneToolsPerPage + row;
		if (toolPos >= toolCount || tools[toolPos] == nullptr)
		{
			mgr.Show(tuneToolNumberButtons[row], false);
			mgr.Show(tuneToolFanButtons[row], false);
			mgr.Show(tuneToolFlowButtons[row], false);
			mgr.Show(tuneToolPaButtons[row], false);
			continue;
		}

		OM::Tool * const tool = tools[toolPos];
		const int toolIndex = tool->index;
		tuneToolNumberText[row].printf("T%d", toolIndex);
		tuneToolNumberButtons[row]->SetText(tuneToolNumberText[row].c_str());
		tuneToolNumberButtons[row]->SetEvent(evNull, toolIndex);
		tuneToolNumberButtons[row]->SetBorderVisible(toolIndex == currentTool);
		mgr.Show(tuneToolNumberButtons[row], true);

		const int fan = tool->fans.IsEmpty() ? -1 : (int)tool->fans.LowestSetBit();
		if (fan >= 0 && fan < (int)TuneMaxFans)
		{
			tuneToolFanText[row].printf("%d%%", tuneFanPercent[fan]);
			tuneToolFanButtons[row]->SetText(tuneToolFanText[row].c_str());
			tuneToolFanButtons[row]->SetEvent(evTuneToolFan, toolIndex);
			mgr.Show(tuneToolFanButtons[row], true);
		}
		else
		{
			mgr.Show(tuneToolFanButtons[row], false);
		}

		const int extruder = tool->extruders.IsEmpty() ? -1 : (int)tool->extruders.LowestSetBit();
		if (extruder >= 0 && extruder < (int)TuneMaxExtruders)
		{
			tuneToolFlowText[row].printf("%d%%", tuneExtruderFactor[extruder]);
			tuneToolFlowButtons[row]->SetText(tuneToolFlowText[row].c_str());
			tuneToolFlowButtons[row]->SetEvent(evTuneToolFlow, toolIndex);
			mgr.Show(tuneToolFlowButtons[row], true);

			if (tunePressureAdvanceValid[extruder])
			{
				tuneToolPaText[row].printf("%.4f", (double)tunePressureAdvance[extruder]);
			}
			else
			{
				tuneToolPaText[row].copy("--");
			}
			tuneToolPaButtons[row]->SetText(tuneToolPaText[row].c_str());
			tuneToolPaButtons[row]->SetEvent(evTunePressureAdvance, toolIndex);
			mgr.Show(tuneToolPaButtons[row], true);
		}
		else
		{
			mgr.Show(tuneToolFlowButtons[row], false);
			mgr.Show(tuneToolPaButtons[row], false);
		}
	}

	mgr.Show(tunePageUpButton, tuneToolPage > 0);
	mgr.Show(tunePageDownButton, tuneToolPage < maxPage);
	RefreshTuneGeneralFans();
}

static void CreateTuneAdjustmentPopups(const ColourScheme& colours)
{
	const Colour pageBg = UTFT::fromRGB(18, 22, 28);
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour neutralBorder = UTFT::fromRGB(59, 67, 79);
	const Colour cancelRed = UTFT::fromRGB(192, 57, 47);
	const Colour confirmGreen = UTFT::fromRGB(164, 214, 94);
	const PixelNumber popupWidth = 610;
	const PixelNumber popupHeight = 460;
	const PixelNumber adjustWidth = 105;
	const PixelNumber adjustHeight = 70;
	const PixelNumber adjustX[4] = { 63, 178, 327, 442 };
	static const int fanDeltas[4] = { -10, -5, 5, 10 };
	static const int speedDeltas[4] = { -10, -5, 5, 10 };
	static const int feedDeltas[4] = { -3, -1, 1, 3 };
	static const int paDeltas[4] = { -10, -2, 2, 10 }; // thousandths
	static const char * const fanLabels[4] = { "-10", "-5", "+5", "+10" };
	static const char * const speedLabels[4] = { "-10", "-5", "+5", "+10" };
	static const char * const feedLabels[4] = { "-3", "-1", "+1", "+3" };
	static const char * const paLabels[4] = { "-0.01", "-0.002", "+0.002", "+0.01" };

	auto addActions = [&](PopupWindow *popup) {
		DisplayField::SetDefaultColours(text, cancelRed);
		ModernIconButton *cancel = new ModernIconButton(335, 150, 140, 80, IconCancel, evTunePopupCancel);
		popup->AddField(cancel);
		DisplayField::SetDefaultColours(text, confirmGreen);
		ModernIconButton *ok = new ModernIconButton(335, 320, 140, 80, IconOk, evTunePopupConfirm);
		popup->AddField(ok);
	};

	// Pressure advance popup, used as the geometry standard for all three popups.
	tunePressureAdvancePopup = new PopupWindow(popupHeight, popupWidth, pageBg, colours.popupBorderColour);
	DisplayField::SetDefaultFont(glcd19x21);
	DisplayField::SetDefaultColours(text, tile);
	tunePressureAdvancePopupTitle = new ModernTextButton(45, 93, 424, 65, "PRESSURE ADVANCE T0", evNull, 0, glcd19x21, true);
	tunePressureAdvancePopupTitle->SetBorderColour(neutralBorder);
	tunePressureAdvancePopup->AddField(tunePressureAdvancePopupTitle);
	DisplayField::SetDefaultColours(text, tile);
	tunePressureAdvancePopupValue = new ModernTextButton(125, 205, 200, 70, "0.0000", evNull, 0, glcd19x21, true);
	tunePressureAdvancePopupValue->SetBorderColour(neutralBorder);
	tunePressureAdvancePopup->AddField(tunePressureAdvancePopupValue);
	for (unsigned int i = 0; i < 4; ++i)
	{
		ModernTextButton *b = new ModernTextButton(215, adjustX[i], adjustWidth, adjustHeight, paLabels[i], evTunePopupAdjustPa, paDeltas[i], glcd19x21, true);
		b->SetBorderColour(neutralBorder);
		tunePressureAdvancePopup->AddField(b);
	}
	addActions(tunePressureAdvancePopup);

	// Fan popup, same button geometry as pressure advance.
	tuneFanPopup = new PopupWindow(popupHeight, popupWidth, pageBg, colours.popupBorderColour);
	DisplayField::SetDefaultColours(text, tile);
	tuneFanPopupTitle = new ModernTextButton(45, 93, 424, 65, "FAN T0", evNull, 0, glcd19x21, true);
	tuneFanPopupTitle->SetBorderColour(neutralBorder);
	tuneFanPopup->AddField(tuneFanPopupTitle);
	DisplayField::SetDefaultColours(text, tile);
	tuneFanPopupValue = new ModernTextButton(125, 205, 200, 70, "0%", evNull, 0, glcd19x21, true);
	tuneFanPopupValue->SetBorderColour(neutralBorder);
	tuneFanPopup->AddField(tuneFanPopupValue);
	for (unsigned int i = 0; i < 4; ++i)
	{
		ModernTextButton *b = new ModernTextButton(215, adjustX[i], adjustWidth, adjustHeight, fanLabels[i], evTunePopupAdjustPercent, fanDeltas[i], glcd19x21, true);
		b->SetBorderColour(neutralBorder);
		tuneFanPopup->AddField(b);
	}
	addActions(tuneFanPopup);

	// Global speed-factor popup, following paneldue_speed_control_popup_mockup.svg.
	tuneSpeedPopup = new PopupWindow(popupHeight, popupWidth, pageBg, colours.popupBorderColour);
	DisplayField::SetDefaultColours(text, tile);
	tuneSpeedPopupTitle = new ModernTextButton(47, 155, 300, 65, "SPEED:", evNull, 0, glcd19x21, true);
	tuneSpeedPopupTitle->SetBorderColour(neutralBorder);
	tuneSpeedPopup->AddField(tuneSpeedPopupTitle);
	DisplayField::SetDefaultColours(text, tile);
	tuneSpeedPopupValue = new ModernTextButton(127, 205, 200, 65, "100%", evNull, 0, glcd19x21, true);
	tuneSpeedPopupValue->SetBorderColour(neutralBorder);
	tuneSpeedPopup->AddField(tuneSpeedPopupValue);
	for (unsigned int i = 0; i < 4; ++i)
	{
		ModernTextButton *b = new ModernTextButton(212, adjustX[i], adjustWidth, adjustHeight, speedLabels[i], evTunePopupAdjustPercent, speedDeltas[i], glcd19x21, true);
		b->SetBorderColour(neutralBorder);
		tuneSpeedPopup->AddField(b);
	}
	DisplayField::SetDefaultColours(text, cancelRed);
	tuneSpeedPopup->AddField(new ModernIconButton(332, 150, 140, 80, IconCancel, evTunePopupCancel));
	DisplayField::SetDefaultColours(text, confirmGreen);
	tuneSpeedPopup->AddField(new ModernIconButton(332, 320, 140, 80, IconOk, evTunePopupConfirm));

	// Feed-rate/flow popup.
	tuneFeedRatePopup = new PopupWindow(popupHeight, popupWidth, pageBg, colours.popupBorderColour);
	DisplayField::SetDefaultColours(text, tile);
	tuneFeedRatePopupTitle = new ModernTextButton(45, 93, 424, 65, "FEED RATE T0", evNull, 0, glcd19x21, true);
	tuneFeedRatePopupTitle->SetBorderColour(neutralBorder);
	tuneFeedRatePopup->AddField(tuneFeedRatePopupTitle);
	DisplayField::SetDefaultColours(text, tile);
	tuneFeedRatePopupValue = new ModernTextButton(125, 205, 200, 70, "100%", evNull, 0, glcd19x21, true);
	tuneFeedRatePopupValue->SetBorderColour(neutralBorder);
	tuneFeedRatePopup->AddField(tuneFeedRatePopupValue);
	for (unsigned int i = 0; i < 4; ++i)
	{
		ModernTextButton *b = new ModernTextButton(215, adjustX[i], adjustWidth, adjustHeight, feedLabels[i], evTunePopupAdjustPercent, feedDeltas[i], glcd19x21, true);
		b->SetBorderColour(neutralBorder);
		tuneFeedRatePopup->AddField(b);
	}
	addActions(tuneFeedRatePopup);
	DisplayField::SetDefaultFont(DEFAULT_FONT);
}

static void CreateStatusTuneTabFields(const ColourScheme& colours)
{
	mgr.SetRoot(baseRoot);
	const Colour pageBg = UTFT::fromRGB(18, 22, 28);
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour muted = UTFT::fromRGB(154, 164, 178);

	DisplayField::SetDefaultFont(glcd19x21);
	DisplayField::SetDefaultColours(muted, pageBg);
	mgr.AddField(new StaticTextField(87, TuneX(118), TuneW(70), TextAlignment::Left, "SPEED:"));
	tuneGeneralFanLabels[0] = new StaticTextField(87, TuneX(338), TuneW(78), TextAlignment::Left, "FAN AUX:");
	tuneGeneralFanLabels[1] = new StaticTextField(87, TuneX(578), TuneW(70), TextAlignment::Left, "FAN CHA:");
	mgr.AddField(tuneGeneralFanLabels[0]);
	mgr.AddField(tuneGeneralFanLabels[1]);
	mgr.AddField(new StaticTextField(153, TuneX(118), TuneW(44), TextAlignment::Left, "Tool:"));
	mgr.AddField(new StaticTextField(153, TuneX(182), TuneW(114), TextAlignment::Left, "Part Cooling:"));
	mgr.AddField(new StaticTextField(153, TuneX(316), TuneW(114), TextAlignment::Left, "Flow Rate:"));
	mgr.AddField(new StaticTextField(153, TuneX(450), TuneW(119), TextAlignment::Left, "Pressure Adv.:"));
	mgr.AddField(new StaticTextField(153, TuneX(589), TuneW(99), TextAlignment::Left, "Z Offset:"));

	DisplayField::SetDefaultColours(text, tile);
	tuneSpeedText.copy("100%");
	tuneSpeedButton = new ModernTextButton(80, TuneX(188), TuneW(130), 46, tuneSpeedText.c_str(), evTuneSpeed, 0, glcd19x21);
	mgr.AddField(tuneSpeedButton);

	for (unsigned int i = 0; i < 2; ++i)
	{
		tuneGeneralFanText[i].copy("0%");
	}
	tuneGeneralFanButtons[0] = new ModernTextButton(80, TuneX(416), TuneW(130), 46, tuneGeneralFanText[0].c_str(), evTuneGeneralFan, 0, glcd19x21);
	tuneGeneralFanButtons[1] = new ModernTextButton(80, TuneX(649), TuneW(130), 46, tuneGeneralFanText[1].c_str(), evTuneGeneralFan, 1, glcd19x21);
	mgr.AddField(tuneGeneralFanButtons[0]);
	mgr.AddField(tuneGeneralFanButtons[1]);

	for (unsigned int row = 0; row < TuneToolsPerPage; ++row)
	{
		const PixelNumber y = 192 + row * 62;
		tuneToolNumberText[row].printf("T%d", row);
		tuneToolFanText[row].copy("0%");
		tuneToolFlowText[row].copy("100%");
		tuneToolPaText[row].copy("--");
		tuneToolNumberButtons[row] = new ModernTextButton(y, TuneX(118), TuneW(44), 56, tuneToolNumberText[row].c_str(), evNull, row, glcd19x21);
		tuneToolNumberButtons[row]->SetBorderColour(colours.popupBorderColour);
		tuneToolFanButtons[row] = new ModernTextButton(y, TuneX(182), TuneW(114), 56, tuneToolFanText[row].c_str(), evTuneToolFan, row, glcd19x21);
		tuneToolFlowButtons[row] = new ModernTextButton(y, TuneX(316), TuneW(114), 56, tuneToolFlowText[row].c_str(), evTuneToolFlow, row, glcd19x21);
		tuneToolPaButtons[row] = new ModernTextButton(y, TuneX(450), TuneW(119), 56, tuneToolPaText[row].c_str(), evTunePressureAdvance, row, glcd19x21);
		mgr.AddField(tuneToolNumberButtons[row]);
		mgr.AddField(tuneToolFanButtons[row]);
		mgr.AddField(tuneToolFlowButtons[row]);
		mgr.AddField(tuneToolPaButtons[row]);
	}

	// Z offset is adjusted live in fixed 0.02mm steps.
	DisplayField::SetDefaultColours(text, tile);
	mgr.AddField(new ModernTextButton(192, TuneX(589), TuneW(99), 76, "+0.02", evTuneZPlus, 0, glcd19x21));
	tuneZOffsetText.copy("0.000");
	tuneZOffsetButton = new ModernTextButton(275, TuneX(589), TuneW(99), 76, tuneZOffsetText.c_str(), evNull, 0, glcd19x21);
	mgr.AddField(tuneZOffsetButton);
	mgr.AddField(new ModernTextButton(358, TuneX(589), TuneW(99), 76, "-0.02", evTuneZMinus, 0, glcd19x21));

	tunePageUpButton = new ModernIconButton(192, TuneX(736), TuneW(54), 118, IconUp, evTunePageUp);
	tunePageDownButton = new ModernIconButton(316, TuneX(736), TuneW(54), 118, IconDown, evTunePageDown);
	mgr.AddField(tunePageUpButton);
	mgr.AddField(tunePageDownButton);

	statusTuneRoot = mgr.GetRoot();
	AddStatusSubTabs(statusTuneRoot);
	// Paint the modern STATUS content background before the tab/content fields.
	mgr.SetRoot(statusTuneRoot);
	DisplayField::SetDefaultColours(text, pageBg);
	mgr.AddField(new ModernTextButton(0, masterTabWidth, DisplayX - masterTabWidth, DisplayY, nullptr, evNull, 0, glcd19x21));
	statusTuneRoot = mgr.GetRoot();
	DisplayField::SetDefaultFont(DEFAULT_FONT);
	CreateTuneAdjustmentPopups(colours);
	RefreshTuneToolRows();
}
#endif

static void CreateMainPages(uint32_t language, const ColourScheme& colours)
{
	if (language >= ARRAY_SIZE(LanguageTables))
	{
		language = 0;
	}
	emptyRoot = mgr.GetRoot();
	mgr.SetLeftMargin(masterTabWidth);
	strings = &LanguageTables[language];
	CreateCommonFields(colours);
	baseRoot = mgr.GetRoot();		// save the root of fields that we usually display

	// Create the fields that are common to the Control and Print pages
	DisplayField::SetDefaultColours(colours.titleBarTextColour, colours.titleBarBackColour);
	mgr.AddField(nameField = new StaticTextField(row1, 0, DisplayX - statusFieldWidth, TextAlignment::Centre, machineName.c_str()));
	mgr.AddField(statusField = new StaticTextField(row1, DisplayX - statusFieldWidth, statusFieldWidth, TextAlignment::Right, nullptr));
	CreateTemperatureGrid(colours);
	commonRoot = mgr.GetRoot();		// save the root of fields that we display on more than one page

	// Create the pages
	CreateControlTabFields(colours);
	CreatePrintingTabFields(colours);
	CreateStatusObjectsTabFields(colours);
	CreateMessageTabFields(colours);
	CreateSetupTabFields(language, colours);

	RelayoutLegacyFields();
	AddControlSubTabs();
	AddStatusSubTabs(printRoot);
#if DISPLAY_X == 800
	CreateControlToolsTabFields(colours);
	CreateControlMovementTabFields(colours);
	CreateControlExtrusionTabFields(colours);
	CreateStatusJobStatusTabFields(colours);
	CreateStatusTuneTabFields(colours);
	CreateStatusJobTabFields(colours);
#endif
#if DISPLAY_X != 800
	AddStatusSubTabs(statusObjectsRoot);
#endif
	AddSystemSubTabs(messageRoot);
	AddSystemSubTabs(setupRoot);
	CreateScreensaverPopup();
	CreateFirmwareUpdatePopup();
}

namespace UI
{
	static void Adjusting(ButtonPress bp)
	{
		fieldBeingAdjusted = bp;
		if (bp == currentButton)
		{
			currentButton.Clear();		// to stop it being released
		}
	}

	static void StopAdjusting()
	{
		if (fieldBeingAdjusted.IsValid())
		{
			mgr.Press(fieldBeingAdjusted, false);
			fieldBeingAdjusted.Clear();
		}
	}

	static void CurrentButtonReleased()
	{
		if (currentButton.IsValid())
		{
			mgr.Press(currentButton, false);
			currentButton.Clear();
		}
	}

	static void ClearAlertOrResponse();

	// Return the number of supported languages
	unsigned int GetNumLanguages()
	{
		return NumLanguages;
	}

	void InitColourScheme(const ColourScheme *scheme)
	{
		colours = scheme;
	}

	// Create all the fields we ever display
	void CreateFields(uint32_t language, const ColourScheme& colours, uint32_t p_infoTimeout)
	{
		infoTimeout = p_infoTimeout;

		// Set up default colours and margins
		mgr.Init(colours.defaultBackColour);
		DisplayField::SetDefaultFont(DEFAULT_FONT);
		ButtonWithText::SetFont(DEFAULT_FONT);
		CharButtonRow::SetFont(DEFAULT_FONT);
		SingleButton::SetTextMargin(textButtonMargin);
		SingleButton::SetIconMargin(iconButtonMargin);

		// Create the pages
		CreateMainPages(language, colours);

		// Create the popup fields
		CreateIntegerAdjustPopup(colours);
		CreateIntegerRPMAdjustPopup(colours);
		CreateMovePopup(colours);
		CreateExtrudePopup(colours);
		fileListPopup = CreateFileListPopup(filesListButtons, filenameButtons, NumFileRows, NumFileColumns, colours, true);
		macrosPopup = CreateFileListPopup(macrosListButtons, macroButtons, NumMacroRows, NumMacroColumns, colours, false);
		CreateFileActionPopup(colours);
		CreateVolumePopup(colours);
		CreateInfoTimeoutPopup(colours);
		CreateScreensaverTimeoutPopup(colours);
		CreateBabystepAmountPopup(colours);
		CreateFeedrateAmountPopup(colours);
		CreateBaudRatePopup(colours);
		CreateColoursPopup(colours);
		CreateAreYouSurePopup(colours);
		CreateKeyboardPopup(language, colours);
		CreateLanguagePopup(colours);
		alertPopup = new AlertPopup(colours);
		CreateBabystepPopup(colours);

		DisplayField::SetDefaultColours(colours.labelTextColour, colours.defaultBackColour);
		touchCalibInstruction = new StaticTextField(DisplayY/2 - 10, 0, DisplayX, TextAlignment::Centre, strings->touchTheSpot);

		mgr.SetRoot(nullptr);

#ifdef SUPPORT_ENCODER
		encoder = new RotaryEncoder(2, 3, 32+6);			// PA2, PA3 and PB6
		encoder->Init(4);
#endif
	}

	// This is called when no job is active/paused
	void ShowFilesButton()
	{
		// First hide everything removed then show everything new
		// otherwise remnants of the to-be-hidden might remain
		mgr.Show(resumeButton,		false);
		mgr.Show(cancelButton,		false);
		mgr.Show(pauseButton,		false);
		mgr.Show(printProgressBar,	false);

		mgr.Show(babystepButton,	true);
		mgr.Show(reprintButton,		lastJobFileNameAvailable);
		mgr.Show(filesButton,		true);
	}

	// This is called when a job is active
	void ShowPauseButton()
	{
		// First hide everything removed then show everything new
		// otherwise remnants of the to-be-hidden might remain
		mgr.Show(resumeButton,		false);
		mgr.Show(cancelButton,		false);
		mgr.Show(filesButton,		false);
		mgr.Show(reprintButton,		false);

		mgr.Show(pauseButton,		true);
		mgr.Show(babystepButton,	true);
		mgr.Show(printProgressBar,	true);
	}

	// This is called when a job is paused
	void ShowResumeAndCancelButtons()
	{
		// First hide everything removed then show everything new
		// otherwise remnants of the to-be-hidden might remain
		mgr.Show(filesButton,		false);
		mgr.Show(pauseButton,		false);
		mgr.Show(reprintButton,		false);
		mgr.Show(babystepButton,	false);

		mgr.Show(cancelButton,		true);
		mgr.Show(resumeButton,		true);
		mgr.Show(printProgressBar,	true);
	}

	// Show or hide an axis on the move button grid and on the axis display
	void ShowAxis(size_t slot, bool b, const char* axisLetter)
	{
		if (slot >= MaxDisplayableAxes)
		{
			return;
		}
		// The table gives us a pointer to the label field, which is followed by 8 buttons. So we need to show or hide 9 fields.
		DisplayField *f = moveAxisRows[slot];
		for (int i = 0; i < 9 && f != nullptr; ++i)
		{
			mgr.Show(f, b);
			if (i > 0) // actual move buttons
			{
				TextButtonForAxis *textButton = static_cast<TextButtonForAxis*>(f);
				textButton->SetAxisLetter(axisLetter[0]);
			}
			f = f->next;
		}
		mgr.Show(controlTabAxisPos[slot], b);
#if DISPLAY_X == 800
		mgr.Show(printTabAxisPos[slot], b);
#endif
		if (numDisplayedAxes < MaxDisplayableAxes)
		{
			mgr.Show(movePopupAxisPos[slot], b);		// the move popup axis positions occupy the last axis row of the move popup
		}
		else
		{
			// This is incremental and we might end up that this row is no longer available
			for (size_t i = 0; i < MaxDisplayableAxes; ++i)
			{
				mgr.Show(movePopupAxisPos[i], false);
			}
		}
	}

	void UpdateAxisPosition(size_t axisIndex, float fval)
	{
		if (axisIndex < MaxTotalAxes)
		{
			auto axis = OM::GetAxis(axisIndex);
			if (axis != nullptr)
			{
				#if DISPLAY_X == 800
				const int moveAxisSlot = GetControlMoveAxisSlot(axis->letter[0]);
				if (moveAxisSlot >= 0)
				{
					controlMovePosition[moveAxisSlot] = fval;
					controlMovePositionValid[moveAxisSlot] = true;
					RefreshControlMovePosition(static_cast<unsigned int>(moveAxisSlot));
				}
				#endif
				if (axis->slot >= MaxDisplayableAxes)
				{
					return;
				}
				size_t slot = axis->slot;

				if (axisMaxVal > 1000)
				{
					controlTabAxisPos[slot]->SetNumDecimals(1);
#if DISPLAY_X == 800
					printTabAxisPos[slot]->SetNumDecimals(1);
#endif
					movePopupAxisPos[slot]->SetNumDecimals(1);
				}

				controlTabAxisPos[slot]->SetValue(fval);
#if DISPLAY_X == 800
				printTabAxisPos[slot]->SetValue(fval);
#endif
				movePopupAxisPos[slot]->SetValue(fval);
			}
		}
	}

	void UpdateCurrentTemperature(size_t heaterIndex, float fval)
	{
#if DISPLAY_X == 800
		if (heaterIndex < JobStatusMaxHeaters)
		{
			jobStatusHeaterTemps[heaterIndex] = fval;
			jobStatusHeaterValid[heaterIndex] = true;
			if (currentUiPage == UiPage::ControlTools)
			{
				RefreshControlToolsPage();
			}
			if (currentUiPage == UiPage::ControlExtrusion)
			{
				RefreshControlExtrudeTools();
			}
			if (currentUiPage == UiPage::StatusJobStatus)
			{
				RefreshJobStatusTilesByType(JobStatusTileType::ToolTemp);
				RefreshJobStatusTilesByType(JobStatusTileType::BedTemp);
				RefreshJobStatusTilesByType(JobStatusTileType::ChamberTemp);
			}
		}
#endif
		OM::Slots heaterSlots;
		OM::GetHeaterSlots(heaterIndex, heaterSlots);
		if (!heaterSlots.IsEmpty())
		{
			const size_t count = heaterSlots.Size();
			for (size_t i = 0; i < count; ++i)
			{
				currentTemps[heaterSlots[i]]->SetValue(fval);
			}

			heaterSlots.Clear();
		}
	}

	void UpdateHeaterStatus(const size_t heaterIndex, const OM::HeaterStatus status)
	{
#if DISPLAY_X == 800
		if (heaterIndex < JobStatusMaxHeaters)
		{
			jobStatusHeaterStatus[heaterIndex] = status;
			if (currentUiPage == UiPage::ControlTools)
			{
				RefreshControlToolsPage();
			}
			if (currentUiPage == UiPage::StatusJobStatus)
			{
				RefreshJobStatusTilesByType(JobStatusTileType::ToolTemp);
				RefreshJobStatusTilesByType(JobStatusTileType::BedTemp);
				RefreshJobStatusTilesByType(JobStatusTileType::ChamberTemp);
			}
		}
#endif
		OM::Slots heaterSlots;
		OM::GetHeaterSlots(heaterIndex, heaterSlots);
		const Colour foregroundColour =	(status == OM::HeaterStatus::fault)
					? colours->errorTextColour
					: colours->infoTextColour;
		const Colour backgroundColour =
					  (status == OM::HeaterStatus::standby) ? colours->standbyBackColour
					: (status == OM::HeaterStatus::active)  ? colours->activeBackColour
					: (status == OM::HeaterStatus::fault)   ? colours->errorBackColour
					: (status == OM::HeaterStatus::tuning)  ? colours->tuningBackColour
					: colours->defaultBackColour;
		if (!heaterSlots.IsEmpty())
		{
			const Colour bedOrChamberBgColor = (backgroundColour == colours->defaultBackColour)
				? colours->buttonImageBackColour
				: backgroundColour;
			const size_t count = heaterSlots.Size();
			for (size_t i = 0; i < count; ++i)
			{
				const size_t slot = heaterSlots[i];
				currentTemps[slot]->SetColours(foregroundColour, backgroundColour);

				// If it's a bed or a chamber we update colors for the tool button as well
				OM::IterateBedsWhile([&heaterIndex, &status, &foregroundColour, &bedOrChamberBgColor, &slot](OM::Bed*& bed, size_t) {
					if (bed->heater == (int)heaterIndex)
					{
						bed->heaterStatus = status;
						toolButtons[slot]->SetColours(foregroundColour, bedOrChamberBgColor);
						return false;	// This will lead to getting out of the iteration on first hit - is that really what we want?
					}
					return true;
				});
				OM::IterateChambersWhile([&heaterIndex, &status, &foregroundColour, &bedOrChamberBgColor, &slot](OM::Chamber*& chamber, size_t) {
					if (chamber->heater == (int)heaterIndex)
					{
						chamber->heaterStatus = status;
						toolButtons[slot]->SetColours(foregroundColour, bedOrChamberBgColor);
						return false;	// This will lead to getting out of the iteration on first hit - is that really what we want?
					}
					return true;
				});
			}
			heaterSlots.Clear();
		}
	}

	void SetCurrentTool(int32_t ival)
	{
		if (ival == currentTool)
		{
			return;
		}
		currentTool = ival;
#if DISPLAY_X == 800
		if (tuneToolNumberButtons[0] != nullptr)
		{
			RefreshTuneToolRows();
		}
		if (jobStatusLabels[0] != nullptr)
		{
			RefreshJobStatusTiles();
		}
		if (controlToolHeaderCards[0] != nullptr)
		{
			RefreshControlToolsPage();
		}
		if (controlExtrudeToolCards[0] != nullptr)
		{
			SelectControlExtrudePageForActiveTool();
			RefreshControlExtrudeTools();
		}
#endif
	}

	enum TimesLeft { file, filament, slicer, max };
	static int timesLeft[TimesLeft::max];
	static uint32_t simulatedTime;
	static uint32_t jobDuration;
	static uint32_t jobWarmUpDuration;
	static String<50> timesLeftText;

	static const char *GetStatusString(OM::PrinterStatus status)
	{
		unsigned int index = (unsigned int)status;
		if (index >= ARRAY_SIZE(strings->statusValues) || !strings->statusValues[index])
		{
			return "unknown status";
		}

		return strings->statusValues[index];
	}

	void ChangeStatus(OM::PrinterStatus oldStatus, OM::PrinterStatus newStatus)
	{

		if (oldStatus != newStatus)
		{
			const char *fromStatus = GetStatusString(oldStatus);
			const char *toStatus = GetStatusString(newStatus);

			MessageLog::AppendMessageF(MessageLog::LogLevel::Verbose,
					"Info: status changed from %s to %s.", fromStatus, toStatus);
		}

		switch (newStatus)
		{
		case OM::PrinterStatus::printing:
		case OM::PrinterStatus::simulating:
			if (oldStatus != OM::PrinterStatus::paused && oldStatus != OM::PrinterStatus::resuming)
			{
				// Starting a new print, so clear the times
				timesLeft[0] = timesLeft[1] = timesLeft[2] = 0;
				simulatedTime = 0;
#if DISPLAY_X == 800
				jobStatusLayer = jobStatusNumLayers = jobStatusProgress = 0;
				jobStatusDuration = 0;
#endif
			}
			SetLastFileSimulated(newStatus == OM::PrinterStatus::simulating);
			if (oldStatus != newStatus)
			{
				PrintStarted();
			}
			[[fallthrough]];
		case OM::PrinterStatus::paused:
		case OM::PrinterStatus::pausing:
		case OM::PrinterStatus::resuming:
			if (currentTab == tabStatus)
			{
				nameField->SetValue(printingFile.c_str());
			}
			break;

		case OM::PrinterStatus::idle:
			printingFile.Clear();
#if DISPLAY_X == 800
			jobStatusLayer = jobStatusNumLayers = jobStatusProgress = 0;
			jobStatusDuration = 0;
			UpdateStatusObjectCount(0);
			currentStatusObject = -1;
			selectedStatusObject = -1;
			statusObjectPage = 0;
#endif
			nameField->SetValue(machineName.c_str());		// if we are on the print tab then it may still be set to the file that was being printed
			if (IsPrintingStatus(oldStatus))
			{
				mgr.Refresh(true);		// Ending a print creates a popup and that will prevent removing some of the elements hidden so force it here
			}
			[[fallthrough]];
		case OM::PrinterStatus::configuring:
			if (oldStatus == OM::PrinterStatus::flashing)
			{
				mgr.ClearAllPopups();						// clear the firmware update message
			}
			break;

		case OM::PrinterStatus::connecting:
			printingFile.Clear();
#if DISPLAY_X == 800
			jobStatusLayer = jobStatusNumLayers = jobStatusProgress = 0;
			jobStatusDuration = 0;
			UpdateStatusObjectCount(0);
			currentStatusObject = -1;
			selectedStatusObject = -1;
			statusObjectPage = 0;
#endif
			mgr.ClearAllPopups();
			break;

		default:
			nameField->SetValue(machineName.c_str());
			break;
		}
#if DISPLAY_X == 800
		if (controlToolHeaderCards[0] != nullptr) RefreshControlToolsPage();
		RefreshJobStatusActions();
		RefreshJobStatusHeader();
		if (newStatus == OM::PrinterStatus::printing || newStatus == OM::PrinterStatus::simulating)
		{
			if (jobStatusThumbnail != nullptr) jobStatusThumbnail->SetChanged();
		}
#endif
	}

	// Append an amount of time to timesLeftText
	static void AppendTimeLeft(int t)
	{
		if (t <= 0)
		{
			timesLeftText.cat(strings->notAvailable);
		}
		else if (t < 60)
		{
			timesLeftText.catf("%ds", t);
		}
		else if (t < 60 * 60)
		{
			timesLeftText.catf("%dm %02ds", t/60, t%60);
		}
		else
		{
			t /= 60;
			timesLeftText.catf("%dh %02dm", t/60, t%60);
		}
	}

	void UpdateTimesLeftText()
	{
		if (!PrintInProgress())
		{
			return;
		}
		size_t count = 0;
		timesLeftText.Clear();
		if (simulatedTime > 0)
		{
			timesLeftText.copy(strings->simulated);
			AppendTimeLeft(simulatedTime + jobWarmUpDuration - jobDuration);
			++count;
		}
		if (timesLeft[TimesLeft::slicer] > 0)
		{
			if (count > 0) {
				timesLeftText.cat(", ");
			}
			timesLeftText.cat(strings->slicer);
			AppendTimeLeft(timesLeft[TimesLeft::slicer]);
			++count;
		}
		if ((count < 2 || (DisplayX >= 800 && count < 3)) && timesLeft[TimesLeft::filament] > 0)
		{
			if (count > 0) {
				timesLeftText.cat(", ");
			}
			timesLeftText.cat(strings->filament);
			AppendTimeLeft(timesLeft[TimesLeft::filament]);
			++count;
		}
		if ((count < 2 || (DisplayX >= 800 && count < 3)) && timesLeft[TimesLeft::file] > 0)
		{
			if (count > 0) {
				timesLeftText.cat(", ");
			}
			timesLeftText.cat(strings->file);
			AppendTimeLeft(timesLeft[TimesLeft::file]);
			++count;
		}

		timeLeftField->SetValue(timesLeftText.c_str());
		mgr.Show(timeLeftField, true);
	}

	void UpdateTimesLeft(size_t index, unsigned int seconds)
	{
		if (index < (int)ARRAY_SIZE(timesLeft))
		{
			timesLeft[index] = seconds;
			UpdateTimesLeftText();
		}
	}

	void UpdateDuration(uint32_t duration)
	{
		jobDuration = duration;
#if DISPLAY_X == 800
		jobStatusDuration = duration;
		if (currentUiPage == UiPage::StatusJobStatus)
		{
			RefreshJobStatusHeader();
		}
#endif
		UpdateTimesLeftText();
	}

	void UpdateWarmupDuration(uint32_t warmupDuration)
	{
		jobWarmUpDuration = warmupDuration;
		UpdateTimesLeftText();
	}

	void SetSimulatedTime(uint32_t simdTime)
	{
		simulatedTime = simdTime;
		UpdateTimesLeftText();
	}

	void SwitchToTab(ButtonBase *newTab) {
		switch (newTab->GetEvent()) {
		case evTabControl:
#if DISPLAY_X == 800
			mgr.SetRoot(controlToolsRoot);
			currentUiPage = UiPage::ControlTools;
			RefreshControlToolsPage();
#else
			mgr.SetRoot(controlRoot);
#endif
			nameField->SetValue(machineName.c_str());
			break;
		case evTabStatus:
#if DISPLAY_X == 800
			mgr.SetRoot(statusJobStatusRoot);
			currentUiPage = UiPage::StatusJobStatus;
			RefreshJobStatusTiles();
			RefreshJobStatusHeader();
			RefreshJobStatusActions();
#else
			mgr.SetRoot(printRoot);
			nameField->SetValue(
					PrintInProgress() ? printingFile.c_str() : machineName.c_str());
#endif
			break;
		case evTabSystem:
			mgr.SetRoot(messageRoot);
			if (keyboardIsDisplayed)
			{
				keyboardDataHandler = SendGcode;
				mgr.SetPopup(keyboardPopup, AutoPlace, keyboardPopupY, false);
			}
			break;
		case evTabMsg:
			mgr.SetRoot(messageRoot);
			if (keyboardIsDisplayed)
			{
				keyboardDataHandler = SendGcode;
				mgr.SetPopup(keyboardPopup, AutoPlace, keyboardPopupY, false);
			}
			break;
		case evTabSetup:
			mgr.SetRoot(setupRoot);
			break;
		default:
			mgr.SetRoot(commonRoot);
			break;
		}
		mgr.Refresh(true);
	}

	// Change to the page indicated. Return true if the page has a permanently-visible button.
	static bool ChangePage(ButtonBase *newTab)
	{
		if (newTab == currentTab)
		{
			mgr.ClearAllPopups();						// if already on the correct page, just clear popups
		}
		else
		{
			if (currentTab != nullptr)
			{
				currentTab->Press(false, 0);			// remove highlighting from the old tab
				if (currentTab->GetEvent() == evTabSetup && nvData.IsSaveNeeded())
				{
					SaveSettings();						// leaving the Control tab and we have changed settings, so save them
				}
			}
			newTab->Press(true, 0);						// highlight the new tab
			currentTab = newTab;
			mgr.ClearAllPopups();
			SwitchToTab(newTab);
		}
		return true;
	}

	void ShowFirmwareUpdatePopup()
	{
		mgr.SetPopup(firmwareUpdatePopup);
	}

	void ActivateScreensaver()
	{
		mgr.Show(screensaverText, isLandscape);
		mgr.SetPopup(screensaverPopup);
		lastScreensaverMoved = SystemTick::GetTickCount();
	}

	bool DeactivateScreensaver()
	{
		if (!screensaverPopup->IsPopupActive())
			return false;

		mgr.ClearPopup(true, screensaverPopup);

		return true;
	}

	void AnimateScreensaver()
	{
		if (SystemTick::GetTickCount() - lastScreensaverMoved >= ScreensaverMoveTime)
		{
			static unsigned int seed = SystemTick::GetTickCount();
			const PixelNumber width = isLandscape ? DisplayX : DisplayXP;
			const PixelNumber height = isLandscape ? DisplayY : DisplayYP;
			const PixelNumber availableWidth = (width - 2*margin - screensaverTextWidth);
			const PixelNumber availableHeight = (height - 2*margin - rowTextHeight);
			const PixelNumber x = (rand_r(&seed) % availableWidth);
			const PixelNumber y = (rand_r(&seed) % availableHeight);
			if (isLandscape)
			{
				mgr.Show(screensaverText, false);
				screensaverText->SetPosition(x + margin, y + margin);
				mgr.Show(screensaverText, true);
			}
			lastScreensaverMoved = SystemTick::GetTickCount();
		}
	}

	// Pop up the keyboard
	void ShowKeyboard()
	{
		keyboardDataHandler = SendGcode;
		mgr.SetPopup(keyboardPopup, AutoPlace, keyboardPopupY);
		keyboardIsDisplayed = true;
	}

	// This is called when the Cancel button on a popup is pressed
	void PopupCancelled()
	{
		if (mgr.GetPopup() == keyboardPopup)
		{
			keyboardIsDisplayed = false;
		}
	}

	// Return true if polling should be performed
	bool IsSetupTab()
	{
		return currentTab == tabSetup;			// don't poll while we are on the Setup page
	}

	void Tick()
	{
#ifdef SUPPORT_ENCODER
		encoder->Poll();
#endif
	}

#ifdef SUPPORT_ENCODER
	void HandleEncoderChange(const int change)
	{
		bool sent = false;
		if (sent) {
			lastEncoderCommandSentAt = SystemTick::GetTickCount();
		}
	}
#endif

	// This is called in the main spin loop
	void Spin()
	{
#ifdef SUPPORT_ENCODER
		if (SystemTick::GetTickCount() - lastEncoderCommandSentAt >= MinimumEncoderCommandInterval)
		{
			// Check encoder and command movement
			const int ch = encoder->GetChange();
			if (ch != 0)
			{
				HandleEncoderChange(ch);
			}
		}
#endif

		if (currentTab == tabMsg)
		{
			MessageLog::UpdateMessages(false);
		}
		if (alertTicks != 0 && SystemTick::GetTickCount() - whenAlertReceived >= alertTicks)
		{
			ClearAlertOrResponse();
		}
#if DISPLAY_X == 800
		const uint32_t now = SystemTick::GetTickCount();
		if (currentUiPage == UiPage::StatusJobStatus && now - jobStatusLastLiveRefresh >= 1000)
		{
			jobStatusLastLiveRefresh = now;
			RefreshJobStatusTiles();
			RefreshJobStatusHeader();
			mgr.Refresh(false);
		}
		if (currentUiPage == UiPage::StatusObjects && statusObjectsDirty)
		{
			RefreshStatusObjectsPage();
			const bool full = statusObjectsNeedFullRefresh;
			statusObjectsDirty = false;
			statusObjectsNeedFullRefresh = false;
			mgr.Refresh(full);
		}
#endif
	}

	// This is called when we have just started a file print
	void PrintStarted()
	{
		if (isLandscape)
		{
			ChangePage(tabStatus);
		}
	}

	// This is called when we have just received the name of the file being printed
	void PrintingFilenameChanged(const char data[])
	{
		if (!printingFile.Similar(data))
		{
			printingFile.copy(data);
			if (currentTab == tabStatus && PrintInProgress())
			{
				nameField->SetChanged();
			}
#if DISPLAY_X == 800
			RefreshJobStatusHeader();
			if (jobStatusThumbnail != nullptr)
			{
				jobStatusThumbnailCard->SetChanged();
				jobStatusThumbnail->SetChanged();
			}
#endif
		}
	}

	void LastJobFileNameAvailable(const bool available)
	{
		lastJobFileNameAvailable = available;
		if (!PrintInProgress())
		{
			mgr.Show(reprintButton, available);
		}
	}

	void SetLastFileSimulated(const bool lastFileSimulated)
	{
		TextButton* redoButton = static_cast<TextButton*>(reprintButton);
		redoButton->SetEvent(lastFileSimulated ? evResimulate : evReprint, 0);
		redoButton->SetText(lastFileSimulated ? strings->resimulate : strings->reprint);
	}

	// This is called just before the main polling loop starts. Display the default page.
	void ShowDefaultPage()
	{
		ChangePage(tabControl);
	}

	// Update the fields that are to do with the printing status
	void UpdatePrintingFields()
	{
		OM::PrinterStatus status = GetStatus();
		if (status == OM::PrinterStatus::printing || status == OM::PrinterStatus::simulating)
		{
			ShowPauseButton();
		}
		else if (status == OM::PrinterStatus::paused)
		{
			ShowResumeAndCancelButtons();
		}
		else
		{
			ShowFilesButton();
		}

		// Don't enable the time left field when we start printing, instead this will get enabled when we receive a suitable message
		if (!PrintInProgress())
		{
			mgr.Show(timeLeftField, false);
		}

		const OM::PrinterStatus stat = GetStatus();
		statusField->SetValue(((unsigned int)stat < ARRAY_SIZE(strings->statusValues) && strings->statusValues[(unsigned int)stat]) ? strings->statusValues[(unsigned int)stat] : "unknown status");
#if DISPLAY_X == 800
		RefreshJobStatusActions();
		RefreshJobStatusHeader();
#endif
	}

	// Set the percentage of print completed
	void SetPrintProgressPercent(unsigned int percent)
	{
		printProgressBar->SetPercent((uint8_t)percent);
#if DISPLAY_X == 800
		jobStatusProgress = constrain<unsigned int>(percent, 0, 100);
		if (currentUiPage == UiPage::StatusJobStatus)
		{
			RefreshJobStatusHeader();
		}
#endif
	}

	// Update the geometry or the number of axes
	void UpdateGeometry(unsigned int p_numAxes, bool p_isDelta)
	{
		if (p_numAxes != numVisibleAxes || p_isDelta != isDelta)
		{
			numVisibleAxes = p_numAxes;
			isDelta = p_isDelta;
			FileManager::RefreshMacrosList();
			numDisplayedAxes = 0;
			OM::IterateAxesWhile([](OM::Axis*& axis, size_t)
			{
				axis->slot = MaxTotalAxes;
				if (!axis->visible)
				{
					return true;
				}
				const char * letter = axis->letter;
				if (numDisplayedAxes < MaxDisplayableAxes)
				{
					axis->slot = numDisplayedAxes;
					++numDisplayedAxes;

					// Update axis letter everywhere we display it
					const uint8_t slot = axis->slot;
					controlTabAxisPos	[slot]->SetLabel(letter);
					moveAxisRows		[slot]->SetValue(letter);
#if DISPLAY_X == 800
					printTabAxisPos		[slot]->SetLabel(letter);
#endif
					movePopupAxisPos	[slot]->SetLabel(letter);
					homeButtons			[slot]->SetText(letter);

					// Update axis letter to be sent for homing commands
					homeButtons[slot]->SetEvent(homeButtons[slot]->GetEvent(), letter);
					homeButtons[slot]->SetColours(colours->buttonTextColour, (axis->homed) ? colours->homedButtonBackColour : colours->notHomedButtonBackColour);

					mgr.Show(homeButtons[slot], !isDelta);
					ShowAxis(slot, true, axis->letter);
				}
				// When we get here it's likely to be the initialisation phase
				// and we won't have the babystep amount set
				if (axis->letter[0] == 'Z')
				{
					babystepOffsetField->SetValue(axis->babystep);
#if DISPLAY_X == 800
					if (tuneZOffsetButton != nullptr)
					{
						tuneZOffsetText.printf("%.3f", (double)axis->babystep);
						tuneZOffsetButton->SetText(tuneZOffsetText.c_str());
					}
#endif
				}
				return true;
			});
			// Hide axes possibly shown before
			for (size_t i = numDisplayedAxes; i < MaxDisplayableAxes; ++i)
			{
				mgr.Show(homeButtons[i], false);
				ShowAxis(i, false);
			}
		}
	}

	void UpdateAllHomed()
	{
		bool allHomed = true;
		OM::IterateAxesWhile([&allHomed](OM::Axis*& axis, size_t) {
			if (axis->visible && !axis->homed)
			{
				allHomed = false;
				return false;
			}
			return true;
		});
		if (allHomed != allAxesHomed)
		{
			allAxesHomed = allHomed;
			homeAllButton->SetColours(colours->buttonTextColour, (allAxesHomed) ? colours->homedButtonBackColour : colours->notHomedButtonBackColour);
		}
		#if DISPLAY_X == 800
		RefreshControlMoveHoming();
		#endif
	}

	// Update the homed status of the specified axis. If the axis is -1 then it represents the "all homed" status.
	void UpdateHomedStatus(size_t axisIndex, bool isHomed)
	{
		OM::Axis *axis = OM::GetOrCreateAxis(axisIndex);
		if (axis == nullptr)
		{
			return;
		}
		axis->homed = isHomed;
		const size_t slot = axis->slot;
		if (slot < MaxDisplayableAxes)
		{
			homeButtons[slot]->SetColours(colours->buttonTextColour, (isHomed) ? colours->homedButtonBackColour : colours->notHomedButtonBackColour);
		}

		UpdateAllHomed();
	}

	// Update the Z probe text
	void UpdateZProbe(const char data[])
	{
		zprobeBuf.copy(data);
		zProbe->SetChanged();
	}

	// Update the machine name
	void UpdateMachineName(const char data[])
	{
		machineName.copy(data);
		nameField->SetChanged();
	}

	// Update the IP address fiels on Setup tab
	void UpdateIP(const char data[])
	{
		ipAddress.copy(data);
		ipAddressField->SetChanged();
	}

	// Update the fan RPM
	void UpdateFanName(size_t fanIndex, const char *name)
	{
		if (fanIndex < TuneMaxFans)
		{
			tuneFanNames[fanIndex].copy((name != nullptr) ? name : "");
			if (currentUiPage == UiPage::StatusTune)
			{
				RefreshTuneGeneralFans();
			}
			if (currentUiPage == UiPage::StatusJobStatus)
			{
				RefreshJobStatusTilesByType(JobStatusTileType::FanAux);
				RefreshJobStatusTilesByType(JobStatusTileType::FanCha);
			}
		}
	}

	void UpdateFanPercent(size_t fanIndex, int rpm)
	{
#if DISPLAY_X == 800
		if (fanIndex < TuneMaxFans)
		{
			tuneFanPercent[fanIndex] = constrain<int>(rpm, 0, 100);
			tuneFanValid[fanIndex] = true;
			if (tuneToolNumberButtons[0] != nullptr)
			{
				RefreshTuneToolRows();
			}
			if (currentUiPage == UiPage::StatusJobStatus)
			{
				RefreshJobStatusTilesByType(JobStatusTileType::FanPart);
				RefreshJobStatusTilesByType(JobStatusTileType::FanAux);
				RefreshJobStatusTilesByType(JobStatusTileType::FanCha);
			}
		}
#endif
		if (currentTool == NoTool)
		{
			if (fanIndex == 0)
			{
				UpdateField(fanSpeed, rpm);
			}
		}
		else
		{
			// There might be multiple tools using the same fan and one of them might
			// be the active one but not necessarily the first one so we need to iterate
			OM::IterateToolsWhile([&fanIndex, &rpm](OM::Tool*& tool, size_t) {
				if (tool->index == currentTool && tool->fans.IsBitSet(fanIndex) && tool->fans.LowestSetBit() == fanIndex)
				{
					UpdateField(fanSpeed, rpm);
				}
				return true;
			});
		}
	}

	void UpdateToolTemp(size_t toolIndex, size_t toolHeaterIndex, int32_t temp, bool active)
	{
		OM::Tool *tool = OM::GetOrCreateTool(toolIndex);

		// If we do not handle this tool back off
		if (tool == nullptr)
		{
			return;
		}

		tool->UpdateTemp(toolHeaterIndex, temp, active);
		if (toolHeaterIndex == 0 || nvData.GetHeaterCombineType() == HeaterCombineType::notCombined)
		{
			if (tool->slot + toolHeaterIndex < MaxSlots)
			{
				UpdateField((active ? activeTemps : standbyTemps)[tool->slot + toolHeaterIndex], temp);
			}
		}
#if DISPLAY_X == 800
		if (toolHeaterIndex == 0 && currentUiPage == UiPage::ControlTools)
		{
			RefreshControlToolsPage();
		}
#endif
	}

	void UpdateTemperature(size_t heaterIndex, int ival, IntegerButton** fields)
	{
		OM::Slots heaterSlots;
		OM::GetHeaterSlots(heaterIndex, heaterSlots, false);	// Ignore tools
		if (!heaterSlots.IsEmpty())
		{
			const size_t count = heaterSlots.Size();
			for (size_t i = 0; i < count; ++i)
			{
				UpdateField(fields[heaterSlots[i]], ival);
			}

			heaterSlots.Clear();
		}
	}

	// Update an active temperature
	void UpdateActiveTemperature(size_t index, int ival)
	{
#if DISPLAY_X == 800
		if (index < ControlToolMaxHeaters)
		{
			controlToolActiveTarget[index] = ival;
			if (currentUiPage == UiPage::ControlTools) RefreshControlToolsPage();
		}
#endif
		UpdateTemperature(index, ival, activeTemps);
	}

	// Update a standby temperature
	void UpdateStandbyTemperature(size_t index, int ival)
	{
#if DISPLAY_X == 800
		if (index < ControlToolMaxHeaters)
		{
			controlToolStandbyTarget[index] = ival;
			if (currentUiPage == UiPage::ControlTools) RefreshControlToolsPage();
		}
#endif
		UpdateTemperature(index, ival, standbyTemps);
	}

#if DISPLAY_X == 800
	void UpdateColdExtrudeTemperature(float value)
	{
		controlColdExtrudeTemperature = value;
		controlColdExtrudeTemperatureValid = true;
	}

	void UpdateColdRetractTemperature(float value)
	{
		controlColdRetractTemperature = value;
		controlColdRetractTemperatureValid = true;
	}
#else
	void UpdateColdExtrudeTemperature(float value) { UNUSED(value); }
	void UpdateColdRetractTemperature(float value) { UNUSED(value); }
#endif

	// Update an extrusion factor
	void UpdateExtrusionFactor(size_t index, int ival)
	{
#if DISPLAY_X == 800
		if (index < TuneMaxExtruders)
		{
			tuneExtruderFactor[index] = ival;
			if (tuneToolNumberButtons[0] != nullptr)
			{
				RefreshTuneToolRows();
			}
			if (currentUiPage == UiPage::StatusJobStatus)
			{
				RefreshJobStatusTilesByType(JobStatusTileType::FlowFactor);
			}
		}
#endif
		OM::IterateToolsWhile([&index, &ival](OM::Tool*& tool, size_t) {
			if (tool->extruders.IsBitSet(index) && tool->slot < MaxSlots)
			{
				UpdateField(extrusionFactors[tool->slot], ival);
			}
			return tool->slot < MaxSlots;
		});
	}

	// Update the print speed factor
	void UpdateSpeedPercent(int ival)
	{
		UpdateField(spd, ival);
#if DISPLAY_X == 800
		tuneSpeedPercent = ival;
		if (tuneSpeedButton != nullptr)
		{
			tuneSpeedText.printf("%d%%", ival);
			tuneSpeedButton->SetText(tuneSpeedText.c_str());
		}
#endif
	}

	void UpdateJobLayer(unsigned int layer)
	{
#if DISPLAY_X == 800
		jobStatusLayer = layer;
		if (currentUiPage == UiPage::StatusJobStatus) RefreshJobStatusHeader();
#else
		UNUSED(layer);
#endif
	}

	void UpdateJobNumLayers(unsigned int layers)
	{
#if DISPLAY_X == 800
		jobStatusNumLayers = layers;
		if (currentUiPage == UiPage::StatusJobStatus) RefreshJobStatusHeader();
#else
		UNUSED(layers);
#endif
	}

	void UpdateCurrentMoveRequestedSpeed(float value)
	{
#if DISPLAY_X == 800
		jobStatusRequestedSpeed = value;
#else
		UNUSED(value);
#endif
	}

	void UpdateCurrentMoveTopSpeed(float value)
	{
#if DISPLAY_X == 800
		jobStatusTopSpeed = value;
#else
		UNUSED(value);
#endif
	}

	void UpdateCurrentMoveExtrusionRate(float value)
	{
#if DISPLAY_X == 800
		jobStatusExtrusionRate = value;
#else
		UNUSED(value);
#endif
	}

	void UpdateFilamentDiameter(size_t extruder, float value)
	{
#if DISPLAY_X == 800
		if (extruder < JobStatusMaxExtruders)
		{
			jobStatusFilamentDiameter[extruder] = value;
			jobStatusFilamentDiameterValid[extruder] = value > 0.0f;
		}
#else
		UNUSED(extruder); UNUSED(value);
#endif
	}

	void UpdatePressureAdvance(size_t index, float value)
	{
#if DISPLAY_X == 800
		if (index < TuneMaxExtruders)
		{
			tunePressureAdvance[index] = value;
			tunePressureAdvanceValid[index] = true;
			if (tuneToolNumberButtons[0] != nullptr)
			{
				RefreshTuneToolRows();
			}
		}
#else
		UNUSED(index);
		UNUSED(value);
#endif
	}

	void UpdateStatusCurrentObject(int objectIndex)
	{
#if DISPLAY_X == 800
		currentStatusObject = objectIndex;
#else
		UNUSED(objectIndex);
#endif
	}

	void UpdateStatusObjectName(size_t objectIndex, const char *name)
	{
#if DISPLAY_X == 800
		if (objectIndex >= StatusMaxObjects) return;
		StatusObjectInfo& obj = statusObjects[objectIndex];
		obj.present = true;
		if (name == nullptr || strcasecmp(name, "null") == 0) obj.name.Clear();
		else obj.name.copy(name);
		statusObjectsDirty = true;
#else
		UNUSED(objectIndex); UNUSED(name);
#endif
	}

	void UpdateStatusObjectCancelled(size_t objectIndex, bool cancelled)
	{
#if DISPLAY_X == 800
		if (objectIndex >= StatusMaxObjects) return;
		statusObjects[objectIndex].present = true;
		statusObjects[objectIndex].cancelled = cancelled;
		statusObjectsDirty = true;
#else
		UNUSED(objectIndex); UNUSED(cancelled);
#endif
	}

	void BeginStatusObjectCoordinate(size_t objectIndex, bool xAxis)
	{
#if DISPLAY_X == 800
		if (objectIndex >= StatusMaxObjects) return;
		StatusObjectInfo& obj = statusObjects[objectIndex];
		obj.present = true;
		if (xAxis) obj.xValid = false;
		else obj.yValid = false;
		statusObjectsDirty = true;
#else
		UNUSED(objectIndex); UNUSED(xAxis);
#endif
	}

	void ClearStatusObjectCoordinate(size_t objectIndex, bool xAxis)
	{
#if DISPLAY_X == 800
		if (objectIndex >= StatusMaxObjects) return;
		StatusObjectInfo& obj = statusObjects[objectIndex];
		obj.present = true;
		if (xAxis) obj.xValid = false;
		else obj.yValid = false;
		statusObjectsDirty = true;
#else
		UNUSED(objectIndex); UNUSED(xAxis);
#endif
	}

	void UpdateStatusObjectCoordinate(size_t objectIndex, bool xAxis, float value)
	{
#if DISPLAY_X == 800
		if (objectIndex >= StatusMaxObjects) return;
		StatusObjectInfo& obj = statusObjects[objectIndex];
		obj.present = true;
		bool& valid = xAxis ? obj.xValid : obj.yValid;
		float& minValue = xAxis ? obj.xMin : obj.yMin;
		float& maxValue = xAxis ? obj.xMax : obj.yMax;
		if (!valid)
		{
			minValue = maxValue = value;
			valid = true;
		}
		else
		{
			if (value < minValue) minValue = value;
			if (value > maxValue) maxValue = value;
		}
		statusObjectsDirty = true;
#else
		UNUSED(objectIndex); UNUSED(xAxis); UNUSED(value);
#endif
	}

	void UpdateStatusObjectCount(size_t count)
	{
#if DISPLAY_X == 800
		const unsigned int newCount = static_cast<unsigned int>((count > StatusMaxObjects) ? StatusMaxObjects : count);
		for (unsigned int i = 0; i < newCount; ++i) statusObjects[i].present = true;
		for (unsigned int i = newCount; i < StatusMaxObjects; ++i)
		{
			statusObjects[i].present = false;
			statusObjects[i].cancelled = false;
			statusObjects[i].xValid = false;
			statusObjects[i].yValid = false;
			statusObjects[i].name.Clear();
		}
		if (statusObjectCount != newCount) statusObjectsNeedFullRefresh = true;
		statusObjectCount = newCount;
		if (selectedStatusObject >= static_cast<int>(newCount)) selectedStatusObject = -1;
		statusObjectsDirty = true;
#else
		UNUSED(count);
#endif
	}

	// Process a new message box alert, clearing any existing one
	void ProcessAlert(const Alert& alert)
	{
		if (isLandscape)
		{
			alertPopup->Set(alert);
			mgr.SetPopup(alertPopup, AutoPlace, AutoPlace);
		}
		alertMode = alert.mode;
		displayingResponse = false;
		whenAlertReceived = SystemTick::GetTickCount();
		alertTicks = (alertMode < 2) ? (uint32_t)(alert.timeout * 1000.0) : 0;
	}

	// Process a command to clear a message box alert
	void ClearAlert()
	{
		if (alertMode >= 0)
		{
			alertTicks = 0;
			mgr.ClearPopup(true, alertPopup);
			CurrentAlertModeClear();
			alertMode = -1;
		}
	}

	// Clear a message box alert or response. Called when the user presses the close button or the alert or response times out.
	void ClearAlertOrResponse()
	{
		if (alertMode >= 0 || displayingResponse)
		{
			alertTicks = 0;
			mgr.ClearPopup(true, alertPopup);
			CurrentAlertModeClear();
			alertMode = -1;
			displayingResponse = false;
		}
	}

	bool CanDimDisplay()
	{
		return alertMode < 2;
	}

	void ProcessSimpleAlert(const char* _ecv_array text)
	{
		if (alertMode < 2)												// if the current alert doesn't require acknowledgement
		{
			if (isLandscape)
			{
				alertPopup->Set(strings->message, text, 1, 0);
				mgr.SetPopup(alertPopup, AutoPlace, AutoPlace);
			}
			alertMode = 1;												// a simple alert is like a mode 1 alert without a title
			displayingResponse = false;
			whenAlertReceived = SystemTick::GetTickCount();
			alertTicks = 0;												// no timeout
		}
	}

	// Process a new response. This is treated like a simple alert except that it times out and isn't cleared by a "clear alert" command from the host.
	void NewResponseReceived(const char* _ecv_array text)
	{
		const bool isErrorMessage = StringStartsWith(text, "Error");
		if (   alertMode < 2											// if the current alert doesn't require acknowledgement
			&& !(currentTab == tabSetup || currentTab == tabMsg)		// don't show on setup tab or on console tab
			&& (isErrorMessage || infoTimeout != 0)
		   )
		{
			if (isLandscape)
			{
				alertPopup->Set(strings->response, text, 1, 0);
				mgr.SetPopup(alertPopup, AutoPlace, AutoPlace);
			}
			alertMode = -1;												// make sure that a call to ClearAlert doesn't clear us
			displayingResponse = true;
			whenAlertReceived = SystemTick::GetTickCount();
			alertTicks = isErrorMessage ? 0 : infoTimeout * SystemTick::TicksPerSecond;				// time out if it isn't an error message
		}
	}

	// This is called when the user selects a new file from a list of SD card files
	void FileSelected(const char * _ecv_array null fileName)
	{
		fpNameField->SetValue(fileName);
		// Clear out the old field values, they relate to the previous file we looked at until we process the response
		fpSizeField->SetValue(0);						// would be better to make it blank
		fpHeightField->SetValue(0.0);					// would be better to make it blank
		fpLayerHeightField->SetValue(0.0);				// would be better to make it blank
		fpFilamentField->SetValue(0);					// would be better to make it blank
		generatedByText.Clear();
		fpGeneratedByField->SetChanged();
		lastModifiedText.Clear();
		fpLastModifiedField->SetChanged();
		printTimeText.Clear();
		fpPrintTimeField->SetChanged();
	}

	// This is called when the "generated by" file information has been received
	void UpdateFileGeneratedByText(const char data[])
	{
		generatedByText.copy(data);
		fpGeneratedByField->SetChanged();
	}

	// This is called when the "last modified" file information has been received
	void UpdateFileLastModifiedText(const char data[])
	{
		lastModifiedText.copy(data);
		lastModifiedText.Replace('T', ' ');
		lastModifiedText.Replace('+', '\0');		// ignore time zone if present
		lastModifiedText.Replace('.', '\0');		// ignore decimal seconds if present (DCS 2.0.0 sends them)
		fpLastModifiedField->SetChanged();
	}

	// This is called when the "last modified" file information has been received
	void UpdatePrintTimeText(uint32_t seconds, bool isSimulated)
	{
		bool update = false;
		if (isSimulated)
		{
			printTimeText.Clear();					// prefer simulated to estimated print time
			fpPrintTimeField->SetLabel(strings->simulatedPrintTime);
			update = true;
		}
		else if (printTimeText.IsEmpty())
		{
			fpPrintTimeField->SetLabel(strings->estimatedPrintTime);
			update = true;
		}
		if (update)
		{
			unsigned int minutes = (seconds + 50)/60;
			printTimeText.printf("%dh %02dm", minutes / 60, minutes % 60);
			fpPrintTimeField->SetChanged();
		}
	}

	// This is called when the object height information for the file has been received
	void UpdateFileObjectHeight(float f)
	{
		fpHeightField->SetValue(f);
	}

	// This is called when the layer height information for the file has been received
	void UpdateFileLayerHeight(float f)
	{
		fpLayerHeightField->SetValue(f);
	}

	// This is called when the size of the file has been received
	void UpdateFileSize(int size)
	{
		fpSizeField->SetValue(size);
	}

	// This is called when the filament needed by the file has been received
	void UpdateFileFilament(int len)
	{
		fpFilamentField->SetValue(len);
	}

	unsigned int GetThumbnailTargetWidth()
	{
#if DISPLAY_X == 800
		if (currentUiPage == UiPage::StatusJobStatus && jobStatusThumbnail != nullptr)
		{
			return jobStatusThumbnail->GetWidth();
		}
#endif
		return (mgr.IsPopupActive(fileDetailPopup) && fpThumbnail != nullptr) ? fpThumbnail->GetWidth() : 0;
	}

	unsigned int GetThumbnailTargetHeight()
	{
#if DISPLAY_X == 800
		if (currentUiPage == UiPage::StatusJobStatus && jobStatusThumbnail != nullptr)
		{
			return jobStatusThumbnail->GetHeight();
		}
#endif
		return (mgr.IsPopupActive(fileDetailPopup) && fpThumbnail != nullptr) ? fpThumbnail->GetHeight() : 0;
	}

	bool UpdateFileThumbnailChunk(const struct Thumbnail &thumbnail, uint32_t pixels_offset, const qoi_rgba_t *pixels, size_t pixels_count)
	{
		DrawDirect *target = nullptr;
#if DISPLAY_X == 800
		if (currentUiPage == UiPage::StatusJobStatus && jobStatusThumbnail != nullptr)
		{
			target = jobStatusThumbnail;
		}
#endif
		if (target == nullptr && mgr.IsPopupActive(fileDetailPopup))
		{
			target = fpThumbnail;
		}
		if (target == nullptr || pixels == nullptr)
		{
			return false;
		}

		// QOI decoding is chunked (currently up to 64 pixels). Convert each chunk immediately
		// to the LCD's native RGB565 format; no full thumbnail framebuffer is allocated.
		uint16_t rgb565[64];
		if (pixels_count > ARRAY_SIZE(rgb565))
		{
			return false;
		}
		for (size_t i = 0; i < pixels_count; ++i)
		{
			const uint8_t r = pixels[i].rgba.r;
			const uint8_t g = pixels[i].rgba.g;
			const uint8_t b = pixels[i].rgba.b;
			rgb565[i] = static_cast<uint16_t>(((r & 0xF8u) << 8) | ((g & 0xFCu) << 3) | (b >> 3));
		}
		target->DrawRect565(thumbnail.width, thumbnail.height, pixels_offset, rgb565, pixels_count);
		return true;
	}

	// Return true if we are displaying file information
	bool IsDisplayingFileInfo()
	{
		return currentFile != nullptr;
	}

	static void DoEmergencyStop()
	{
		// We send M112 for the benefit of old firmware, and F0 0F (an invalid UTF8 sequence) for new firmware
		SerialIo::Sendf("M112 ;" "\xF0" "\x0F" "\n");
		TouchBeep();											// needed when we are called from ProcessTouchOutsidePopup
		Delay(1000);
		SerialIo::Sendf("M999\n");
		Delay(1000);
	}

	// Make this into a template if we need something else than IntegerButton** as list
	size_t GetButtonSlot(IntegerButton** buttonList, ButtonBase* button)
	{
		size_t slot = MaxSlots;
		for (size_t i = 0; i < MaxSlots; ++i)
		{
			if (buttonList[i] == button)
			{
				slot = i;
				break;
			}
		}
		return slot;
	}

	void ProcessRelease(ButtonPress bp)
	{
		if (!bp.IsValid())
		{
			return;
		}

		ButtonBase *f = bp.GetButton();
		Event ev = (Event)(f->GetEvent());

		switch(ev)
		{
		case evTabControl:
		case evTabStatus:
		case evTabSystem:
		case evTabMsg:
		case evTabSetup:

		case evExtrudeAmount:
		case evExtrudeRate:

		case evAdjustBaudRate:
		case evAdjustVolume:
		case evAdjustInfoTimeout:
		case evAdjustScreensaverTimeout:
		case evAdjustBabystepAmount:
		case evAdjustFeedrate:
		case evAdjustColours:
		case evAdjustLanguage:
			break;
		case evOkAlert:
		case evCloseAlert:
		case evChoiceAlert:
			mgr.Press(bp, false);
			ClearAlertOrResponse();
			break;
		default:
			mgr.Press(bp, false);
			break;
		}
	}

	// Process a touch event
	void ProcessTouch(ButtonPress bp)
	{
		if (bp.IsValid())
		{
			ButtonBase *f = bp.GetButton();
			currentButton = bp;
			mgr.Press(bp, true);
			Event ev = (Event)(f->GetEvent());


			if (bp.GetEvent() != evAdjustVolume)
			{
				TouchBeep();		// give audible feedback of the touch, unless adjusting the volume
			}

			switch(ev)
			{
			case evEmergencyStop:
				DoEmergencyStop();
				break;

			case evTabControl:
			case evTabStatus:
			case evTabSystem:
			case evTabMsg:
			case evTabSetup:
				if (ChangePage(f))
				{
					currentButton.Clear();						// keep the button highlighted after it is released
				}
				break;

			case evControlTools:
#if DISPLAY_X == 800
				mgr.SetRoot(controlToolsRoot);
				currentUiPage = UiPage::ControlTools;
				RefreshControlToolsPage();
#else
				mgr.SetRoot(controlRoot);
#endif
				mgr.Refresh(true);
				currentButton.Clear();
				break;

			case evControlMovement:
#if DISPLAY_X == 800
				mgr.SetRoot(controlMovementRoot);
				RefreshControlMoveSteps();
				RefreshControlMoveHoming();
#else
				mgr.SetRoot(controlRoot);
#endif
				currentUiPage = UiPage::ControlMovement;
				mgr.Refresh(true);
				currentButton.Clear();
				break;

			case evControlExtrusion:
#if DISPLAY_X == 800
				mgr.SetRoot(controlExtrusionRoot);
				SelectControlExtrudePageForActiveTool();
				RefreshControlExtrudeTools();
				RefreshControlExtrudeSelections();
#else
				mgr.SetRoot(controlRoot);
#endif
				currentUiPage = UiPage::ControlExtrusion;
				mgr.Refresh(true);
				currentButton.Clear();
				break;

			case evControlMacros:
				mgr.SetRoot(controlRoot);
				currentUiPage = UiPage::ControlMacros;
				mgr.Refresh(true);
				currentButton.Clear();
				break;

#if DISPLAY_X == 800
			case evControlToolsPageUp:
				if (controlToolPage > 0) --controlToolPage;
				RefreshControlToolsPage();
				mgr.Refresh(false);
				currentButton.Clear();
				break;

			case evControlToolsPageDown:
				++controlToolPage;
				RefreshControlToolsPage();
				mgr.Refresh(false);
				currentButton.Clear();
				break;

			case evControlToolsActiveTemp:
				OpenControlTempNumpad(static_cast<unsigned int>(bp.GetIParam()), true);
				currentButton.Clear();
				break;

			case evControlToolsStandbyTemp:
				OpenControlTempNumpad(static_cast<unsigned int>(bp.GetIParam()), false);
				currentButton.Clear();
				break;

			case evControlToolsPower:
				HandleControlToolPower(static_cast<unsigned int>(bp.GetIParam()));
				currentButton.Clear();
				break;

			case evNumericKey:
				if (controlTempNumpadPopup != nullptr && mgr.GetPopup() == controlTempNumpadPopup)
				{
					const unsigned int digit = static_cast<unsigned int>(bp.GetIParam());
					if (digit <= 9)
					{
						if (controlTempNumpadFresh)
						{
							controlTempNumpadValue = digit;
							controlTempNumpadFresh = false;
						}
						else if (controlTempNumpadValue <= 99)
						{
							controlTempNumpadValue = controlTempNumpadValue * 10 + digit;
						}
						RefreshControlTempNumpadValue();
						mgr.GetPopup()->Refresh(false);
					}
				}
				currentButton.Clear();
				break;

			case evNumericBack:
				if (controlTempNumpadPopup != nullptr && mgr.GetPopup() == controlTempNumpadPopup)
				{
					controlTempNumpadFresh = false;
					controlTempNumpadValue /= 10;
					RefreshControlTempNumpadValue();
					mgr.GetPopup()->Refresh(false);
				}
				currentButton.Clear();
				break;

			case evNumericOk:
				if (controlTempNumpadPopup != nullptr && mgr.GetPopup() == controlTempNumpadPopup)
				{
					SendControlTemperatureTarget();
					mgr.ClearPopup();
				}
				currentButton.Clear();
				break;

			case evNumericCancel:
				if (controlTempNumpadPopup != nullptr && mgr.GetPopup() == controlTempNumpadPopup)
				{
					mgr.ClearPopup();
				}
				currentButton.Clear();
				break;

			case evControlToolChangeConfirm:
				if (GetStatus() != OM::PrinterStatus::printing && GetStatus() != OM::PrinterStatus::simulating)
				{
					SerialIo::Sendf("T%d\n", controlToolChangeTarget);
				}
				mgr.ClearPopup();
				currentButton.Clear();
				break;

			case evControlToolChangeCancel:
				mgr.ClearPopup();
				currentButton.Clear();
				break;

			case evControlMoveStep:
				{
					const int step = bp.GetIParam();
					if (step >= 0 && step < static_cast<int>(ControlMoveStepCount))
					{
						controlMoveSelectedStep = static_cast<unsigned int>(step);
						RefreshControlMoveSteps();
						mgr.Refresh(false);
					}
					currentButton.Clear();
				}
				break;

			case evControlMoveJog:
				{
					const int jog = bp.GetIParam();
					if (jog >= 0 && jog < 6)
					{
						const unsigned int axisSlot = static_cast<unsigned int>(jog / 2);
						OM::Axis * const axis = GetControlMoveAxis(axisSlot);
						if (axis == nullptr || !axis->homed)
						{
							String<64> message;
							message.printf("Printer %c AXIS not homed", "XYZ"[axisSlot]);
							ShowModernAlert(message.c_str());
						}
						else
						{
							const char sign = ((jog & 1) != 0) ? '+' : '-';
							SerialIo::Sendf("G91 G1 %c%c%s F%d G90\n", "XYZ"[axisSlot], sign,
								controlMoveStepText[controlMoveSelectedStep], nvData.GetFeedrate());
						}
					}
					currentButton.Clear();
				}
				break;

			case evControlMoveHome:
				{
					const int axisSlot = bp.GetIParam();
					if (axisSlot == 3)
					{
						// RRF dispatches bare G28 to sys/homeall.g.
						SerialIo::Sendf("G28\n");
					}
					else if (axisSlot >= 0 && axisSlot < static_cast<int>(ControlMoveAxisCount))
					{
						// RRF dispatches G28 X0/Y0/Z0 to homex.g/homey.g/homez.g.
						SerialIo::Sendf("G28 %c0\n", "XYZ"[axisSlot]);
					}
					currentButton.Clear();
				}
				break;

			case evControlMoveBedComp:
				// RRF executes the configured sys/bed.g sequence for G32.
				SerialIo::Sendf("G32\n");
				currentButton.Clear();
				break;

			case evModernAlertClose:
				mgr.ClearPopup();
				currentButton.Clear();
				break;

			case evControlExtrudeSpeed:
				{
					const int index = bp.GetIParam();
					if (index >= 0 && index < static_cast<int>(ControlExtrudeSpeedCount))
					{
						controlExtrudeSelectedSpeed = static_cast<unsigned int>(index);
						RefreshControlExtrudeSelections();
						mgr.Refresh(false);
					}
					currentButton.Clear();
				}
				break;

			case evControlExtrudeDistance:
				{
					const int index = bp.GetIParam();
					if (index >= 0 && index < static_cast<int>(ControlExtrudeDistanceCount))
					{
						controlExtrudeSelectedDistance = static_cast<unsigned int>(index);
						RefreshControlExtrudeSelections();
						mgr.Refresh(false);
					}
					currentButton.Clear();
				}
				break;

			case evControlExtrudeAction:
				SendControlExtrudeAction(bp.GetIParam() < 0);
				currentButton.Clear();
				break;

			case evControlExtrudePageUp:
				if (controlExtrudeToolPage > 0)
				{
					--controlExtrudeToolPage;
					RefreshControlExtrudeTools();
					mgr.Refresh(false);
				}
				currentButton.Clear();
				break;

			case evControlExtrudePageDown:
				{
					const unsigned int total = CountControlExtrudeTools();
					if ((controlExtrudeToolPage + 1) * ControlExtrudeToolsPerPage < total)
					{
						++controlExtrudeToolPage;
						RefreshControlExtrudeTools();
						mgr.Refresh(false);
					}
					currentButton.Clear();
				}
				break;
#endif

			case evStatusJobStatus:
#if DISPLAY_X == 800
				mgr.SetRoot(statusJobStatusRoot);
				RefreshJobStatusTiles();
				RefreshJobStatusHeader();
				RefreshJobStatusActions();
#else
				mgr.SetRoot(printRoot);
#endif
				currentUiPage = UiPage::StatusJobStatus;
				mgr.Refresh(true);
				currentButton.Clear();
				break;

#if DISPLAY_X == 800
			case evStatusJobStatusPauseResume:
				{
					const OM::PrinterStatus stat = GetStatus();
					if (stat == OM::PrinterStatus::printing)
					{
						OpenJobStatusConfirmation(JobStatusConfirmAction::Pause);
					}
					else if (stat == OM::PrinterStatus::paused)
					{
						OpenJobStatusConfirmation(JobStatusConfirmAction::Resume);
					}
					currentButton.Clear();
				}
				break;

			case evStatusJobStatusAbort:
				if (PrintInProgress())
				{
					OpenJobStatusConfirmation(JobStatusConfirmAction::Abort);
				}
				currentButton.Clear();
				break;

			case evStatusJobStatusConfirm:
				switch (jobStatusConfirmAction)
				{
				case JobStatusConfirmAction::Pause: SerialIo::Sendf("M25\n"); break;
				case JobStatusConfirmAction::Resume: SerialIo::Sendf("M24\n"); break;
				case JobStatusConfirmAction::Abort: SerialIo::Sendf("M0\n"); break;
				default: break;
				}
				jobStatusConfirmAction = JobStatusConfirmAction::None;
				mgr.ClearPopup();
				currentButton.Clear();
				break;

			case evStatusJobStatusCancel:
				jobStatusConfirmAction = JobStatusConfirmAction::None;
				mgr.ClearPopup();
				currentButton.Clear();
				break;
#endif

			case evStatusTune:
#if DISPLAY_X == 800
				mgr.SetRoot(statusTuneRoot);
				RefreshTuneToolRows();
#else
				mgr.SetRoot(printRoot);
#endif
				currentUiPage = UiPage::StatusTune;
				mgr.Refresh(true);
				currentButton.Clear();
				break;

#if DISPLAY_X == 800
			case evTuneSpeed:
				OpenTuneSpeedPopup();
				currentButton.Clear();
				break;

			case evTuneGeneralFan:
				{
					const int slot = bp.GetIParam();
					if (slot >= 0 && slot < 2 && tuneGeneralFanIndices[slot] >= 0)
					{
						OpenTuneFanPopup(slot == 0 ? "FAN AUX" : "FAN CHA", -1, tuneGeneralFanIndices[slot]);
					}
					currentButton.Clear();
				}
				break;

			case evTuneToolFan:
				{
					const int toolIndex = bp.GetIParam();
					OM::Tool * const tool = OM::GetTool(toolIndex);
					if (tool != nullptr && !tool->fans.IsEmpty())
					{
						String<24> title;
						title.printf("FAN T%d", toolIndex);
						OpenTuneFanPopup(title.c_str(), toolIndex, tool->fans.LowestSetBit());
					}
					currentButton.Clear();
				}
				break;

			case evTuneToolFlow:
				{
					const int toolIndex = bp.GetIParam();
					OM::Tool * const tool = OM::GetTool(toolIndex);
					if (tool != nullptr && !tool->extruders.IsEmpty())
					{
						OpenTuneFeedPopup(toolIndex, tool->extruders.LowestSetBit());
					}
					currentButton.Clear();
				}
				break;

			case evTunePressureAdvance:
				{
					const int toolIndex = bp.GetIParam();
					OM::Tool * const tool = OM::GetTool(toolIndex);
					if (tool != nullptr && !tool->extruders.IsEmpty())
					{
						OpenTunePressureAdvancePopup(toolIndex, tool->extruders.LowestSetBit());
					}
					currentButton.Clear();
				}
				break;

			case evTunePageUp:
				if (tuneToolPage > 0)
				{
					--tuneToolPage;
					RefreshTuneToolRows();
					mgr.Refresh(false);
				}
				currentButton.Clear();
				break;

			case evTunePageDown:
				++tuneToolPage;
				RefreshTuneToolRows();
				mgr.Refresh(false);
				currentButton.Clear();
				break;

			case evTuneZPlus:
				SerialIo::Sendf("M290 Z0.02\n");
				currentButton.Clear();
				break;

			case evTuneZMinus:
				SerialIo::Sendf("M290 Z-0.02\n");
				currentButton.Clear();
				break;

			case evTunePopupAdjustPercent:
				tunePopupPercent = constrain<int>(tunePopupPercent + bp.GetIParam(), 0, 200);
				if (tunePopupKind == TunePopupKind::Fan)
				{
					tunePopupPercent = constrain<int>(tunePopupPercent, 0, 100);
				}
				UpdateTunePopupValue();
				mgr.GetPopup()->Refresh(false);
				currentButton.Clear();
				break;

			case evTunePopupAdjustPa:
				tunePopupPa += (float)bp.GetIParam() / 1000.0f;
				if (tunePopupPa < 0.0f)
				{
					tunePopupPa = 0.0f;
				}
				UpdateTunePopupValue();
				mgr.GetPopup()->Refresh(false);
				currentButton.Clear();
				break;

			case evTunePopupConfirm:
				switch (tunePopupKind)
				{
				case TunePopupKind::Speed:
					SerialIo::Sendf("M220 S%d\n", tunePopupPercent);
					break;
				case TunePopupKind::Fan:
					if (tunePopupResource >= 0)
					{
						SerialIo::Sendf("M106 P%d S%.3f\n", tunePopupResource, (double)tunePopupPercent / 100.0);
					}
					break;
				case TunePopupKind::Flow:
					if (tunePopupResource >= 0)
					{
						SerialIo::Sendf("M221 D%d S%d\n", tunePopupResource, tunePopupPercent);
					}
					break;
				case TunePopupKind::PressureAdvance:
					if (tunePopupResource >= 0)
					{
						SerialIo::Sendf("M572 D%d S%.4f\n", tunePopupResource, (double)tunePopupPa);
					}
					break;
				default:
					break;
				}
				mgr.ClearPopup();
				tunePopupKind = TunePopupKind::None;
				currentButton.Clear();
				break;

			case evTunePopupCancel:
				mgr.ClearPopup();
				tunePopupKind = TunePopupKind::None;
				currentButton.Clear();
				break;
#endif

			case evStatusJob:
#if DISPLAY_X == 800
				mgr.SetRoot(statusJobRoot);
				FileManager::DisplayFilesPage();
#else
				mgr.SetRoot(printRoot);
#endif
				currentUiPage = UiPage::StatusJob;
				mgr.Refresh(true);
				currentButton.Clear();
				break;

#if DISPLAY_X == 800
			case evStatusJobFile:
				{
					const char * const fileName = bp.GetSParam();
					if (fileName == nullptr)
					{
						ErrorBeep();
						break;
					}
					if (fileName[0] == '*')
					{
						FileManager::RequestFilesPageSubdir(fileName + 1);
					}
					else
					{
						currentFile = fileName;
						statusJobStartFileField->SetText(currentFile);
						mgr.SetPopup(statusJobStartPopup, AutoPlace, AutoPlace);
					}
					currentButton.Clear();
				}
				break;

			case evStatusJobPageUp:
				if (statusJobCanScrollEarlier)
				{
					FileManager::ScrollFilesPage(-static_cast<int>(StatusJobRows));
				}
				else if (statusJobInSubdir)
				{
					FileManager::RequestFilesPageParentDir();
				}
				currentButton.Clear();
				break;

			case evStatusJobPageDown:
				if (statusJobCanScrollLater)
				{
					FileManager::ScrollFilesPage(static_cast<int>(StatusJobRows));
				}
				currentButton.Clear();
				break;

			case evStatusJobPrintConfirm:
				mgr.ClearPopup();
				if (currentFile != nullptr)
				{
					SerialIo::Sendf("M32 ");
					SerialIo::SendFilename(CondStripDrive(StripPrefix(FileManager::GetFilesDir())), currentFile);
					SerialIo::SendChar('\n');
					PrintingFilenameChanged(currentFile);
					currentFile = nullptr;
					CurrentButtonReleased();
					PrintStarted();
				}
				currentButton.Clear();
				break;

			case evStatusJobPrintCancel:
				currentFile = nullptr;
				mgr.ClearPopup();
				currentButton.Clear();
				break;
#endif

			case evStatusObjects:
				mgr.SetRoot(statusObjectsRoot);
				currentUiPage = UiPage::StatusObjects;
#if DISPLAY_X == 800
				if (selectedStatusObject < 0 && currentStatusObject >= 0 &&
					currentStatusObject < static_cast<int>(statusObjectCount))
				{
					selectedStatusObject = currentStatusObject;
					statusObjectPage = static_cast<unsigned int>(currentStatusObject) / StatusObjectsPerPage;
				}
				RefreshStatusObjectsPage();
				statusObjectsDirty = false;
				statusObjectsNeedFullRefresh = false;
#endif
				mgr.Refresh(true);
				currentButton.Clear();
				break;

#if DISPLAY_X == 800
			case evStatusObjectSelect:
				{
					const unsigned int index = statusObjectPage * StatusObjectsPerPage + static_cast<unsigned int>(bp.GetIParam());
					SelectStatusObject(index, false);
					currentButton.Clear();
				}
				break;

			case evStatusObjectNumber:
				{
					const unsigned int index = statusObjectPage * StatusObjectsPerPage + static_cast<unsigned int>(bp.GetIParam());
					OpenStatusObjectCancelPopup(index);
					currentButton.Clear();
				}
				break;

			case evStatusObjectMarker:
				SelectStatusObject(static_cast<unsigned int>(bp.GetIParam()), true);
				currentButton.Clear();
				break;

			case evStatusObjectPageUp:
				if (statusObjectPage > 0)
				{
					--statusObjectPage;
					RefreshStatusObjectsPage();
					mgr.Refresh(true);
				}
				currentButton.Clear();
				break;

			case evStatusObjectPageDown:
				if ((statusObjectPage + 1) * StatusObjectsPerPage < statusObjectCount)
				{
					++statusObjectPage;
					RefreshStatusObjectsPage();
					mgr.Refresh(true);
				}
				currentButton.Clear();
				break;

			case evStatusObjectCancelConfirm:
				mgr.ClearPopup();
				if (pendingStatusObjectCancel >= 0 &&
					pendingStatusObjectCancel < static_cast<int>(statusObjectCount) &&
					!statusObjects[pendingStatusObjectCancel].cancelled)
				{
					SerialIo::Sendf("M486 P%d\n", pendingStatusObjectCancel);
				}
				pendingStatusObjectCancel = -1;
				currentButton.Clear();
				break;

			case evStatusObjectCancelClose:
				pendingStatusObjectCancel = -1;
				mgr.ClearPopup();
				currentButton.Clear();
				break;
#else
			case evStatusObject1:
				selectedStatusObject = statusObjectPage * StatusObjectsPerPage + 0;
				currentButton.Clear();
				break;
			case evStatusObject2:
				selectedStatusObject = statusObjectPage * StatusObjectsPerPage + 1;
				currentButton.Clear();
				break;
			case evStatusObject3:
				selectedStatusObject = statusObjectPage * StatusObjectsPerPage + 2;
				currentButton.Clear();
				break;
			case evStatusObject4:
				selectedStatusObject = statusObjectPage * StatusObjectsPerPage + 3;
				currentButton.Clear();
				break;
			case evStatusObject5:
				selectedStatusObject = statusObjectPage * StatusObjectsPerPage + 4;
				currentButton.Clear();
				break;
			case evStatusObject6:
				selectedStatusObject = statusObjectPage * StatusObjectsPerPage + 5;
				currentButton.Clear();
				break;
			case evStatusObjectPageUp:
				if (statusObjectPage > 0) --statusObjectPage;
				currentButton.Clear();
				break;
			case evStatusObjectPageDown:
				++statusObjectPage;
				currentButton.Clear();
				break;
#endif

			case evSystemConsole:
				mgr.SetRoot(messageRoot);
				currentUiPage = UiPage::SystemConsole;
				mgr.Refresh(true);
				currentButton.Clear();
				break;

			case evSystemAlerts:
				mgr.SetRoot(messageRoot);
				currentUiPage = UiPage::SystemAlerts;
				mgr.Refresh(true);
				currentButton.Clear();
				break;

			case evSystemSettings:
				mgr.SetRoot(setupRoot);
				currentUiPage = UiPage::SystemSettings;
				mgr.Refresh(true);
				currentButton.Clear();
				break;


			case evAdjustToolActiveTemp:
			case evAdjustToolStandbyTemp:
			case evAdjustBedActiveTemp:
			case evAdjustBedStandbyTemp:
			case evAdjustChamberActiveTemp:
			case evAdjustChamberStandbyTemp:
				if (static_cast<IntegerButton*>(f)->GetValue() < 0)
				{
					static_cast<IntegerButton*>(f)->SetValue(0);
				}
				Adjusting(bp);
				if (isLandscape)
				{
					mgr.SetPopup(setTempPopup, AutoPlace, popupY);
				}
				break;

			case evAdjustActiveRPM:
				Adjusting(bp);
				if (isLandscape)
				{
					mgr.SetPopup(setRPMPopup, AutoPlace, popupY);
				}
				break;

			case evAdjustSpeed:
			case evExtrusionFactor:
			case evAdjustFan:
				oldIntValue = static_cast<IntegerButton*>(bp.GetButton())->GetValue();
				Adjusting(bp);
				if (isLandscape)
				{
					mgr.SetPopup(setTempPopup, AutoPlace, popupY);
				}
				break;

			case evSetInt:
				if (fieldBeingAdjusted.IsValid())
				{
					int val = static_cast<const IntegerButton*>(fieldBeingAdjusted.GetButton())->GetValue();
					const event_t eventOfFieldBeingAdjusted = fieldBeingAdjusted.GetEvent();
					switch (eventOfFieldBeingAdjusted)
					{
					case evAdjustBedActiveTemp:
					case evAdjustChamberActiveTemp:
						{
							int index = fieldBeingAdjusted.GetIParam();
							const bool isBed = eventOfFieldBeingAdjusted == evAdjustBedActiveTemp;
							SerialIo::Sendf("%s P%d S%d\n", isBed ? "M140" : "M141", index, val);
						}
						break;

					case evAdjustBedStandbyTemp:
					case evAdjustChamberStandbyTemp:
						{
							int index = fieldBeingAdjusted.GetIParam();
							const bool isBed = eventOfFieldBeingAdjusted == evAdjustBedStandbyTemp;
							SerialIo::Sendf("%s P%d R%d\n", isBed ? "M140" : "M141", index, val);
						}
						break;

					case evAdjustToolActiveTemp:
						{
							int toolNumber = fieldBeingAdjusted.GetIParam();
							OM::Tool* tool = OM::GetTool(toolNumber);
							if (tool == nullptr)
							{
								break;
							}

							const bool useM568 = GetFirmwareFeatures().IsBitSet(m568TempAndRPM);
							if (nvData.GetHeaterCombineType() == HeaterCombineType::combined)
							{
								tool->UpdateTemp(0, val, true);
								SerialIo::Sendf("%s P%d S%d\n", (useM568 ? "M568" : "G10"), toolNumber, tool->heaters[0]->activeTemp);
							}
							else
							{

								// Find the slot for this button to determine which heater index it is
								{
									size_t slot = GetButtonSlot(activeTemps, fieldBeingAdjusted.GetButton());
									if (slot >= MaxSlots || (slot - tool->slot) >= MaxSlots)
									{
										break;
									}
									tool->UpdateTemp(slot - tool->slot, val, true);
								}

								String<maxUserCommandLength> heaterTemps;
								if (tool->GetHeaterTemps(heaterTemps.GetRef(), true))
								{
									SerialIo::Sendf("%s P%d S%s\n", (useM568 ? "M568" : "G10"), toolNumber, heaterTemps.c_str());
								}
							}
						}
						break;

					case evAdjustToolStandbyTemp:
						{
							int toolNumber = fieldBeingAdjusted.GetIParam();
							OM::Tool* tool = OM::GetTool(toolNumber);
							if (tool == nullptr)
							{
								break;
							}

							const bool useM568 = GetFirmwareFeatures().IsBitSet(m568TempAndRPM);
							if (nvData.GetHeaterCombineType() == HeaterCombineType::combined)
							{
								tool->UpdateTemp(0, val, false);
								SerialIo::Sendf("%s P%d R%d\n", (useM568 ? "M568" : "G10"), toolNumber, tool->heaters[0]->standbyTemp);
							}
							else
							{

								// Find the slot for this button to determine which heater index it is
								{
									size_t slot = GetButtonSlot(standbyTemps, fieldBeingAdjusted.GetButton());
									if (slot >= MaxSlots || (slot - tool->slot) >= MaxSlots)
									{
										break;
									}
									tool->UpdateTemp(slot - tool->slot, val, false);
								}

								String<maxUserCommandLength> heaterTemps;
								if (tool->GetHeaterTemps(heaterTemps.GetRef(), false))
								{
									SerialIo::Sendf("%s P%d R%s\n", (useM568 ? "M568" : "G10"), toolNumber, heaterTemps.c_str());
								}
							}
						}
						break;

					case evAdjustActiveRPM:
						{
							auto spindle = OM::GetSpindle(fieldBeingAdjusted.GetIParam());
							if (val == 0)
							{
								SerialIo::Sendf("M5 P%d\n", spindle->index);
							}
							else
							{
								SerialIo::Sendf("M%d P%d S%d\n", val < 0 ? 4 : 3, spindle->index, abs(val));
							}
						}
						break;

					case evExtrusionFactor:
						{
							const int extruder = fieldBeingAdjusted.GetIParam();
							SerialIo::Sendf("M221 D%d S%d\n", extruder, val);
						}
						break;

					case evAdjustFan:
						SerialIo::Sendf("M106 S%d\n", (256 * val)/100);
						break;

					default:
						{
							const char* null cmd = fieldBeingAdjusted.GetSParam();
							if (cmd != nullptr)
							{
								SerialIo::Sendf("%s%d\n", cmd, val);
							}
						}
						break;
					}
					mgr.ClearPopup();
					StopAdjusting();
				}
				break;

			case evAdjustInt:
				if (fieldBeingAdjusted.IsValid())
				{
					IntegerButton *ib = static_cast<IntegerButton*>(fieldBeingAdjusted.GetButton());
					const int change = bp.GetIParam();
					int newValue = ib->GetValue() + change;
					switch(fieldBeingAdjusted.GetEvent())
					{
					case evAdjustToolActiveTemp:
					case evAdjustToolStandbyTemp:
					case evAdjustBedActiveTemp:
					case evAdjustBedStandbyTemp:
					case evAdjustChamberActiveTemp:
					case evAdjustChamberStandbyTemp:
						newValue = constrain<int>(newValue, 0, 1600);		// some users want to print at high temperatures
						break;

					case evAdjustFan:
						newValue = constrain<int>(newValue, 0, 100);
						break;

					case evAdjustActiveRPM:
						{
							auto spindle = OM::GetSpindle(fieldBeingAdjusted.GetIParam());
							newValue = constrain<int>(newValue, -spindle->max, spindle->max);

							// If a change will lead us below the min speed for spindle skip to the other side
							if (newValue > (int)-spindle->min && newValue < (int)spindle->min)
							{
								newValue = (change < 0) ? -spindle->min : spindle->min;
							}
						}
						break;

					default:
						break;
					}
					ib->SetValue(newValue);
				}
				break;

			case evMovePopup:
				mgr.SetPopup(movePopup, AutoPlace, AutoPlace);
				break;

			case evMoveSelectAxis:
				{
					alertPopup->ChangeLetter(bp.GetIParam());
				}
				break;
			case evMoveAxis:
				{
					TextButtonForAxis *textButton = static_cast<TextButtonForAxis*>(bp.GetButton());
					const char letter = textButton->GetAxisLetter();
					SerialIo::Sendf("G91 G1 %s%c%s F%d G90\n", islower(letter) ? "'" : "", letter, bp.GetSParam(), nvData.GetFeedrate());
				}
				break;

			case evExtrudePopup:
				if (isLandscape)
				{
					mgr.SetPopup(extrudePopup, AutoPlace, AutoPlace);
				}
				break;

			case evExtrudeAmount:
				mgr.Press(currentExtrudeAmountPress, false);
				mgr.Press(bp, true);
				currentExtrudeAmountPress = bp;
				currentButton.Clear();						// stop it being released by the timer
				break;

			case evExtrudeRate:
				mgr.Press(currentExtrudeRatePress, false);
				mgr.Press(bp, true);
				currentExtrudeRatePress = bp;
				currentButton.Clear();						// stop it being released by the timer
				break;

			case evExtrude:
			case evRetract:
				if (currentExtrudeAmountPress.IsValid() && currentExtrudeRatePress.IsValid())
				{
					SerialIo::Sendf("M120 M83 G1 E%s%s F%s M121\n",
							(ev == evRetract ? "-" : ""),
							currentExtrudeAmountPress.GetSParam(),
							currentExtrudeRatePress.GetSParam());
				}
				break;

			case evBabyStepPopup:
				mgr.SetPopup(babystepPopup, AutoPlace, AutoPlace);
				break;

			case evBabyStepMinus:
			case evBabyStepPlus:
				{
					SerialIo::Sendf("M290 Z%s%s\n", (ev == evBabyStepMinus ? "-" : ""), babystepAmounts[nvData.GetBabystepAmountIndex()]);
					float currentBabystepAmount = babystepOffsetField->GetValue();
					if (ev == evBabyStepMinus)
					{
						currentBabystepAmount -= babystepAmountsF[nvData.GetBabystepAmountIndex()];
					}
					else
					{
						currentBabystepAmount += babystepAmountsF[nvData.GetBabystepAmountIndex()];
					}
					babystepOffsetField->SetValue(currentBabystepAmount);
				}
				break;

			case evListFiles:
				FileManager::DisplayFilesList();
				break;

			case evListMacros:
				FileManager::DisplayMacrosList();
				break;

			case evCalTouch:
				CalibrateTouch();
				break;

			case evFactoryReset:
				PopupAreYouSure(ev, strings->confirmFactoryReset);
				break;

			case evSelectBed:
				{
					int bedIndex = bp.GetIParam();
					const OM::Bed* bed = OM::GetBed(bedIndex);
					if (bed == nullptr || bed->slot >= MaxSlots)
					{
						break;
					}
					const auto slot = bed->slot;
					if (bed->heaterStatus == OM::HeaterStatus::active)			// if bed is active
					{
						SerialIo::Sendf("M144 P%d\n", bedIndex);
					}
					else
					{
						SerialIo::Sendf("M140 P%d S%d\n", bedIndex, activeTemps[slot]->GetValue());
					}
				}
				break;

			case evSelectChamber:
				{
					const int chamberIndex = bp.GetIParam();
					const OM::Chamber* chamber = OM::GetChamber(chamberIndex);
					if (chamber == nullptr || chamber->slot >= MaxSlots)
					{
						break;
					}
					const auto slot = chamber->slot;
					SerialIo::Sendf("M141 P%d S%d\n",
							chamberIndex,
							(chamber->heaterStatus == OM::HeaterStatus::active ? -274 : activeTemps[slot]->GetValue()));
				}
				break;

			case evSelectHead:
				{
					int head = bp.GetIParam();
					// pressing a evSeelctHead button in the middle of active printing is almost always accidental (and fatal to the print job)
					if (GetStatus() != OM::PrinterStatus::printing && GetStatus() != OM::PrinterStatus::simulating)
					{
						if (head == currentTool)		// if head is active
						{
							SerialIo::Sendf("T-1\n");
						}
						else
						{
							SerialIo::Sendf("T%d\n", head);
						}
					}
				}
				break;

			case evFile:
				{
					const char * _ecv_array fileName = bp.GetSParam();
					if (fileName != nullptr)
					{
						if (fileName[0] == '*')
						{
							// It's a directory
							FileManager::RequestFilesSubdir(fileName + 1);
							//??? need to pop up a "wait" box here
						}
						else
						{
							// It's a regular file
							currentFile = fileName;
							FileSelected(currentFile);
							mgr.SetPopup(fileDetailPopup, AutoPlace, AutoPlace);
						}
					}
					else
					{
						ErrorBeep();
					}
				}
				break;

			case evFilesUp:
				FileManager::RequestFilesParentDir();
				break;

			case evMacrosUp:
				FileManager::RequestMacrosParentDir();
				break;

			case evMacro:
			case evMacroControlPage:
				{
					const char *fileName = bp.GetSParam();
					if (fileName != nullptr)
					{
						if (fileName[0] == '*')		// if it's a directory
						{
							FileManager::RequestMacrosSubdir(fileName + 1);
							//??? need to pop up a "wait" box here
						}
						else
						{
							SerialIo::Sendf("M98 P");
							const char * _ecv_array const dir = (ev == evMacroControlPage) ? FileManager::GetMacrosRootDir() : FileManager::GetMacrosDir();
							SerialIo::SendFilename(CondStripDrive(dir), fileName);
							SerialIo::SendChar('\n');
						}
					}
					else
					{
						ErrorBeep();
					}
				}
				break;

			case evPrintFile:
			case evSimulateFile:
				mgr.ClearPopup();			// clear the file info popup
				mgr.ClearPopup();			// clear the file list popup
				if (currentFile != nullptr)
				{
					SerialIo::Sendf((ev == evSimulateFile) ? "M37 P" : "M32 ");
					SerialIo::SendFilename(CondStripDrive(StripPrefix(FileManager::GetFilesDir())), currentFile);
					SerialIo::SendChar('\n');
					PrintingFilenameChanged(currentFile);
					currentFile = nullptr;							// allow the file list to be updated
					CurrentButtonReleased();
					PrintStarted();
				}
				break;

			case evReprint:
			case evResimulate:
				if (lastJobFileNameAvailable)
				{
					SerialIo::Sendf("%s{job.lastFileName}\n", (ev == evResimulate) ? "M37 P" : "M32 ");
					CurrentButtonReleased();
					PrintStarted();
				}
				break;

			case evCancel:
				eventToConfirm = evNull;
				currentFile = nullptr;
				CurrentButtonReleased();
				PopupCancelled();
				mgr.ClearPopup();
				break;

			case evDeleteFile:
				CurrentButtonReleased();
				PopupAreYouSure(ev, strings->confirmFileDelete);
				break;

			case evSendCommand:
			case evPausePrint:
			case evResumePrint:
			case evReset:
				SerialIo::Sendf("%s\n", bp.GetSParam());
				break;

			case evHomeAxis:
				{
					const char letter = bp.GetSParam()[0];
					SerialIo::Sendf("G28 %s%c0\n", islower(letter) ? "'" : "", letter);
				}
				break;

			case evScrollFiles:
				FileManager::ScrollFiles(bp.GetIParam() * NumFileRows);
				break;

			case evScrollMacros:
				FileManager::ScrollMacros(bp.GetIParam() * NumMacroRows);
				break;

			case evChangeCard:
				(void)FileManager::NextCard();
				break;

			case evKeyboard:
				ShowKeyboard();
				break;

			case evInvertX:
				MirrorDisplay();
				CalibrateTouch();
				break;

			case evInvertY:
				InvertDisplay();
				CalibrateTouch();
				break;

			case evSetBaudRate:
				Adjusting(bp);
				mgr.SetPopup(baudPopup, AutoPlace, popupY);
				break;

			case evAdjustBaudRate:
				{
					const int rate = bp.GetIParam();
					SetBaudRate(rate);
					baudRateButton->SetValue(rate);
				}
				CurrentButtonReleased();
				mgr.ClearPopup();
				StopAdjusting();
				break;

			case evSetVolume:
				Adjusting(bp);
				mgr.SetPopup(volumePopup, AutoPlace, popupY);
				break;

			case evSetInfoTimeout:
				Adjusting(bp);
				mgr.SetPopup(infoTimeoutPopup, AutoPlace, popupY);
				break;

			case evSetScreensaverTimeout:
				Adjusting(bp);
				mgr.SetPopup(screensaverTimeoutPopup, AutoPlace, popupY);
				break;

			case evSetBabystepAmount:
				Adjusting(bp);
				mgr.SetPopup(babystepAmountPopup, AutoPlace, popupY);
				break;

			case evSetFeedrate:
				Adjusting(bp);
				mgr.SetPopup(feedrateAmountPopup, AutoPlace, popupY);
				break;

			case evSetColours:
				if (coloursPopup != nullptr)
				{
					Adjusting(bp);
					mgr.SetPopup(coloursPopup, AutoPlace, popupY);
				}
				break;

			case evBrighter:
			case evDimmer:
				ChangeBrightness(ev == evBrighter);
				break;

			case evAdjustVolume:
				{
					const int newVolume = bp.GetIParam();
					nvData.SetVolume(newVolume);
					volumeButton->SetValue(newVolume);
				}
				TouchBeep();									// give audible feedback of the touch at the new volume level
				break;

			case evAdjustInfoTimeout:
				{
					infoTimeout = bp.GetIParam();
					nvData.SetInfoTimeout(infoTimeout);
					infoTimeoutButton->SetValue(infoTimeout);
				}
				TouchBeep();									// give audible feedback of the touch at the new volume level
				break;

			case evAdjustScreensaverTimeout:
				{
					uint32_t screensaverTimeout = bp.GetIParam();
					nvData.SetScreensaverTimeout(screensaverTimeout * 1000);
					screensaverTimeoutButton->SetValue(screensaverTimeout);
				}
				TouchBeep();									// give audible feedback of the touch at the new volume level
				break;

			case evAdjustBabystepAmount:
				{
					uint32_t babystepAmountIndex = bp.GetIParam();
					nvData.SetBabystepAmountIndex(babystepAmountIndex);
					babystepAmountButton->SetText(babystepAmounts[babystepAmountIndex]);
					babystepMinusButton->SetText(babystepAmounts[babystepAmountIndex]);
					babystepPlusButton->SetText(babystepAmounts[babystepAmountIndex]);
				}
				TouchBeep();									// give audible feedback of the touch at the new volume level
				break;

			case evAdjustFeedrate:
				{
					uint32_t feedrate = bp.GetIParam();
					nvData.SetFeedrate(feedrate);
					feedrateAmountButton->SetValue(feedrate);
				}
				TouchBeep();									// give audible feedback of the touch at the new volume level
				break;

			case evAdjustColours:
				{
					const uint8_t newColours = (uint8_t)bp.GetIParam();
					if (nvData.SetColourScheme(newColours))
					{
						SaveSettings();
						Reset();
					}
				}
				mgr.ClearPopup();
				break;

			case evSetLanguage:
				Adjusting(bp);
				mgr.SetPopup(languagePopup, AutoPlace, popupY);
				break;

			case evAdjustLanguage:
				{
					const uint8_t newLanguage = (uint8_t)bp.GetIParam();
					if (nvData.SetLanguage(newLanguage))
					{
						SaveSettings();
						Reset();
					}
				}
				mgr.ClearPopup();
				break;

			case evSetDimmingType:
				ChangeDisplayDimmerType();
				dimmingTypeButton->SetText(strings->displayDimmingNames[(unsigned int)nvData.GetDisplayDimmerType()]);
				break;

			case evSetHeaterCombineType:
				ChangeHeaterCombineType();
				heaterCombiningButton->SetText(strings->heaterCombineTypeNames[(unsigned int)nvData.GetHeaterCombineType()]);
				break;

			case evSetLogLevel:
				{
					MessageLog::LogLevel logLevel = MessageLog::LogLevelGet();

					logLevel = (MessageLog::LogLevel)(((int)logLevel + 1) % (int)MessageLog::LogLevel::NumTypes);

					logLevelButton->SetText(strings->logLevelNames[(unsigned int)logLevel]);

					nvData.SetLogLevel(logLevel);

					MessageLog::LogLevelSet(logLevel);
				}
				break;

			case evYes:
				CurrentButtonReleased();
				mgr.ClearPopup();								// clear the yes/no popup
				switch (eventToConfirm)
				{
				case evFactoryReset:
					FactoryReset();
					break;

				case evDeleteFile:
					if (currentFile != nullptr)
					{
						mgr.ClearPopup();						// clear the file info popup
						SerialIo::Sendf("M30 ");
						SerialIo::SendFilename(CondStripDrive(StripPrefix(FileManager::GetFilesDir())), currentFile);
						SerialIo::SendChar('\n');
						FileManager::RefreshFilesList();
						currentFile = nullptr;
					}
					break;

				default:
					break;
				}
				eventToConfirm = evNull;
				currentFile = nullptr;
				break;

			case evKey:
				if (!userCommandBuffers[currentUserCommandBuffer].cat((char)bp.GetIParam()))
				{
					userCommandField->SetChanged();
				}
				break;

			case evShift:
				{
					size_t rowOffset;
					if (keyboardShifted)
					{
						bp.GetButton()->Press(false, 0);
						rowOffset = 0;
					}
					else
					{
						rowOffset = 4;
					}
					for (size_t i = 0; i < 4; ++i)
					{
						keyboardRows[i]->ChangeText(currentKeyboard[i + rowOffset]);
					}
				}
				keyboardShifted = !keyboardShifted;
				currentButton.Clear();				// make the key sticky
				break;

			case evBackspace:
				if (!userCommandBuffers[currentUserCommandBuffer].IsEmpty())
				{
					userCommandBuffers[currentUserCommandBuffer].Erase(userCommandBuffers[currentUserCommandBuffer].strlen() - 1);
					userCommandField->SetChanged();
				}
				break;

			case evUp: // TODO new events for moving editor one left or right
				currentHistoryBuffer = (currentHistoryBuffer + numUserCommandBuffers - 1) % numUserCommandBuffers;
				if (currentHistoryBuffer == currentUserCommandBuffer)
				{
					userCommandBuffers[currentUserCommandBuffer].Clear();
				}
				else
				{
					userCommandBuffers[currentUserCommandBuffer].copy(userCommandBuffers[currentHistoryBuffer].c_str());
				}
				userCommandField->SetChanged();
				break;

			case evDown:
				currentHistoryBuffer = (currentHistoryBuffer + 1) % numUserCommandBuffers;
				if (currentHistoryBuffer == currentUserCommandBuffer)
				{
					userCommandBuffers[currentUserCommandBuffer].Clear();
				}
				else
				{
					userCommandBuffers[currentUserCommandBuffer].copy(userCommandBuffers[currentHistoryBuffer].c_str());
				}
				userCommandField->SetChanged();
				break;

			case evSendKeyboardCommand:
				if (userCommandBuffers[currentUserCommandBuffer].strlen() != 0)
				{
					if (keyboardDataHandler)
					{
						keyboardDataHandler(userCommandBuffers[currentUserCommandBuffer].c_str());
					}

					// Add the command to the history if it was different frmo the previous command
					size_t prevBuffer = (currentUserCommandBuffer + numUserCommandBuffers - 1) % numUserCommandBuffers;
					if (strcmp(userCommandBuffers[currentUserCommandBuffer].c_str(), userCommandBuffers[prevBuffer].c_str()) != 0)
					{
						currentUserCommandBuffer = (currentUserCommandBuffer + 1) % numUserCommandBuffers;
					}
					currentHistoryBuffer = currentUserCommandBuffer;
					userCommandBuffers[currentUserCommandBuffer].Clear();
					userCommandField->SetLabel(userCommandBuffers[currentUserCommandBuffer].c_str());
				}
				break;

			case evOkAlert:
				alertPopup->ProcessOkButton();
				break;

			case evCloseAlert:
				SerialIo::Sendf("%s\n", bp.GetSParam());
				break;

			case evChoiceAlert:
				alertPopup->ProcessChoice(bp.GetIParam());
				break;

			case evEditAlert:
				keyboardDataHandler = PopupEditData;
				mgr.SetPopup(keyboardPopup, AutoPlace, keyboardPopupY);
				keyboardIsDisplayed = true;
				break;

			default:
				break;
			}
		}
	}

	// Process a touch event outside the popup on the field being adjusted
	void ProcessTouchOutsidePopup(ButtonPress bp)
	{
		if (!IsSetupTab())
		{
			return;
		}

		if (bp == fieldBeingAdjusted)
		{
			TouchBeep();
			switch(fieldBeingAdjusted.GetEvent())
			{
			case evAdjustSpeed:
			case evExtrusionFactor:
			case evAdjustFan:
				static_cast<IntegerButton*>(fieldBeingAdjusted.GetButton())->SetValue(oldIntValue);
				mgr.ClearPopup();
				StopAdjusting();
				break;

			case evAdjustToolActiveTemp:
			case evAdjustToolStandbyTemp:
			case evAdjustBedActiveTemp:
			case evAdjustBedStandbyTemp:
			case evAdjustChamberActiveTemp:
			case evAdjustChamberStandbyTemp:
			case evAdjustActiveRPM:
			case evSetBaudRate:
			case evSetVolume:
			case evSetInfoTimeout:
			case evSetScreensaverTimeout:
			case evSetFeedrate:
			case evSetBabystepAmount:
			case evSetColours:
				mgr.ClearPopup();
				StopAdjusting();
				break;

			case evSetLanguage:
				mgr.ClearPopup();
				StopAdjusting();
				break;
			}
		}
		else
		{
			switch(bp.GetEvent())
			{
			case evEmergencyStop:
				mgr.Press(bp, true);
				DoEmergencyStop();
				mgr.Press(bp, false);
				break;

			case evTabControl:
			case evTabStatus:
			case evTabMsg:
			case evTabSetup:
				StopAdjusting();
				TouchBeep();
				{
					ButtonBase *btn = bp.GetButton();
					if (ChangePage(btn))
					{
						currentButton.Clear();						// keep the button highlighted after it is released
					}
				}
				break;

			case evSetBaudRate:
			case evSetVolume:
			case evSetInfoTimeout:
			case evSetScreensaverTimeout:
			case evSetFeedrate:
			case evSetBabystepAmount:
			case evSetColours:
			case evSetLanguage:
			case evCalTouch:
			case evInvertX:
			case evInvertY:
			case evFactoryReset:
				// On the Setup tab, we allow any other button to be pressed to exit the current popup
				StopAdjusting();
				mgr.ClearPopup();
				ProcessTouch(bp);
				break;

			default:
				break;
			}
		}
	}

	// This is called when a button press times out
	void OnButtonPressTimeout()
	{
		if (currentButton.IsValid())
		{
			CurrentButtonReleased();
		}
	}

	void DisplayFilesPopup(int cardNumber, unsigned int numVolumes)
	{
		filePopupTitleField->SetValue(cardNumber);
		mgr.Show(changeCardButton, numVolumes > 1);

		if (isLandscape)
		{
			for (size_t i = 0; i < ARRAY_SIZE(filenameButtons); i++)
			{
				filenameButtons[i]->Press(false, 0);
				filenameButtons[i]->Show(false);
			}
			fileListPopupNoFiles->Show(true);
			mgr.SetPopup(fileListPopup, AutoPlace, AutoPlace);
		}
	}

	void FileListCardButtonUpdate(unsigned int numVolumes)
	{
		mgr.Show(changeCardButton, numVolumes > 1);
	}

	void DisplayMacrosPopup()
	{
		if (isLandscape)
		{
			for (size_t i = 0; i < ARRAY_SIZE(macroButtons); i++)
			{
				macroButtons[i]->Press(false, 0);
				macroButtons[i]->Show(false);
			}
			mgr.SetPopup(macrosPopup, AutoPlace, AutoPlace);
		}
	}

	void FileListLoaded(bool filesNotMacros, int errCode)
	{
		FileListButtons& buttons = (filesNotMacros) ? filesListButtons : macrosListButtons;
		if (errCode == 0)
		{
			mgr.Show(buttons.errorField, false);
		}
		else
		{
			buttons.errorField->SetValue(errCode);
			mgr.Show(buttons.errorField, true);
		}
	}

	void EnableFileNavButtons(bool filesNotMacros, bool scrollEarlier, bool scrollLater, bool parentDir)
	{
		FileListButtons& buttons = (filesNotMacros) ? filesListButtons : macrosListButtons;
		mgr.Show(buttons.scrollLeftButton, scrollEarlier);
		mgr.Show(buttons.scrollRightButton, scrollLater);
		mgr.Show(buttons.folderUpButton, parentDir);
	}

	// Update the specified button in the file or macro buttons list. If 'text' is nullptr then hide the button, else display it.
	void EnableStatusJobNavButtons(bool scrollEarlier, bool scrollLater, bool parentDir)
	{
#if DISPLAY_X == 800
		statusJobCanScrollEarlier = scrollEarlier;
		statusJobCanScrollLater = scrollLater;
		statusJobInSubdir = parentDir;
		if (statusJobPageUpButton != nullptr)
		{
			mgr.Show(statusJobPageUpButton, scrollEarlier || parentDir);
		}
		if (statusJobPageDownButton != nullptr)
		{
			mgr.Show(statusJobPageDownButton, scrollLater);
		}
#else
		UNUSED(scrollEarlier);
		UNUSED(scrollLater);
		UNUSED(parentDir);
#endif
	}

	void UpdateStatusJobFileButton(unsigned int buttonIndex, const char * _ecv_array null text, const char * _ecv_array null param)
	{
#if DISPLAY_X == 800
		if (buttonIndex >= StatusJobRows || statusJobFileButtons[buttonIndex] == nullptr)
		{
			return;
		}

		ModernTextButton * const button = statusJobFileButtons[buttonIndex];
		const bool isDirectory = (text != nullptr && text[0] == '*');
		button->SetText(isDirectory ? text + 1 : text);
		button->SetEvent((text == nullptr) ? evNull : evStatusJobFile, param);
		button->SetBorderVisible(isDirectory);
		if (isDirectory)
		{
			button->SetBorderColour(UTFT::fromRGB(59, 67, 79));
		}
		mgr.Show(button, text != nullptr);
#else
		UNUSED(buttonIndex);
		UNUSED(text);
		UNUSED(param);
#endif
	}

	void UpdateFileButton(bool filesNotMacros, unsigned int buttonIndex, const char * _ecv_array null text, const char * _ecv_array null param)
	{
		if (filesNotMacros && text)
		{
			fileListPopupNoFiles->Show(false);
		}

		if (buttonIndex < ((filesNotMacros) ? NumDisplayedFiles : NumDisplayedMacros))
		{
			TextButton * const f = ((filesNotMacros) ? filenameButtons : macroButtons)[buttonIndex];
			f->SetText(text);
			f->SetEvent((text == nullptr) ? evNull : (filesNotMacros) ? evFile : evMacro, param);
			mgr.Show(f, text != nullptr);
		}
	}

	// Update the specified button in the macro short list. If 'fileName' is nullptr then hide the button, else display it.
	// Return true if this should be called again for the next button.
	bool UpdateMacroShortList(unsigned int buttonIndex, const char * _ecv_array null fileName)
	{
#if (DISPLAY_X == 480)
		const bool tooFewSpace = numToolColsUsed >= (MaxSlots - 1);
#else
		const bool tooFewSpace = numToolColsUsed > (MaxSlots - 2);
#endif

		if (buttonIndex >= ARRAY_SIZE(controlPageMacroButtons) || numToolColsUsed == 0 || tooFewSpace)
		{
			return false;
		}

		String<controlPageMacroTextLength>& str = controlPageMacroText[buttonIndex];
		str.Clear();
		const bool isFile = (fileName != nullptr);
		if (isFile)
		{
			str.copy(fileName);
		}
		TextButton * const f = controlPageMacroButtons[buttonIndex];
		f->SetText(SkipDigitsAndUnderscore(str.c_str()));
		f->SetEvent((isFile) ? evMacroControlPage : evNull, str.c_str());
		mgr.Show(f, isFile);
		return true;
	}

	unsigned int GetNumScrolledFiles(bool filesNotMacros)
	{
		return (filesNotMacros) ? NumFileRows : NumMacroRows;
	}

	void AdjustControlPageMacroButtons()
	{
		const unsigned int n = numToolColsUsed;

		if (n != numHeaterAndToolColumns)
		{
			numHeaterAndToolColumns = n;

			// Adjust the width of the control page macro buttons, or hide them completely if insufficient room
			PixelNumber controlPageMacroButtonsColumn = (PixelNumber)(((tempButtonWidth + fieldSpacing) * n) + bedColumn + fieldSpacing);
			PixelNumber controlPageMacroButtonsWidth = (PixelNumber)((controlPageMacroButtonsColumn >= DisplayX - margin) ? 0 : DisplayX - margin - controlPageMacroButtonsColumn);
			if (controlPageMacroButtonsWidth > maxControlPageMacroButtonsWidth)
			{
				controlPageMacroButtonsColumn += controlPageMacroButtonsWidth - maxControlPageMacroButtonsWidth;
				controlPageMacroButtonsWidth = maxControlPageMacroButtonsWidth;
			}

			bool showControlPageMacroButtons = controlPageMacroButtonsWidth >= minControlPageMacroButtonsWidth;

			for (TextButton *& b : controlPageMacroButtons)
			{
				if (showControlPageMacroButtons)
				{
					b->SetPositionAndWidth(controlPageMacroButtonsColumn, controlPageMacroButtonsWidth);
				}
				mgr.Show(b, showControlPageMacroButtons);
			}

			if (currentTab == tabControl)
			{
				mgr.Refresh(true);
			}
		}
	}

	void ResetToolAndHeaterStates() noexcept
	{
		for (size_t i = 0; i < numToolColsUsed; ++i)
		{
			toolButtons[i]->SetColours(colours->buttonTextColour, colours->buttonImageBackColour);
			currentTemps[i]->SetColours(colours->infoTextColour, colours->defaultBackColour);
		}
	}

	void ManageCurrentActiveStandbyFields(
			size_t& slot,
			const bool showCurrent = false,
			const Event activeEvent = evNull,
			const int activeEventValue = -1,
			const Event standbyEvent = evNull,
			const int standbyEventValue = -1
			)
	{
		mgr.Show(currentTemps[slot], showCurrent);
		mgr.Show(activeTemps[slot], activeEvent != evNull);
		mgr.Show(standbyTemps[slot], standbyEvent != evNull);

		activeTemps[slot]->SetEvent(activeEvent, activeEventValue);
		activeTemps[slot]->SetValue(0);
		standbyTemps[slot]->SetEvent(standbyEvent, standbyEventValue);
		standbyTemps[slot]->SetValue(0);
	}

	size_t AddBedOrChamber(OM::BedOrChamber *bedOrChamber, size_t &slot, const bool isBed = true) {
		const size_t count = (isBed ? OM::GetBedCount() : OM::GetChamberCount());
		bedOrChamber->slot = MaxSlots;
		if (slot < MaxSlots && bedOrChamber->heater > -1) {
			bedOrChamber->slot = slot;
			mgr.Show(toolButtons[slot], true);
			ManageCurrentActiveStandbyFields(
					slot,
					true,
					isBed ? evAdjustBedActiveTemp : evAdjustChamberActiveTemp,
					bedOrChamber->index,
					isBed ? evAdjustBedStandbyTemp : evAdjustChamberStandbyTemp,
					bedOrChamber->index
					);
			mgr.Show(extrusionFactors[slot], false);
			toolButtons[slot]->SetEvent(isBed ? evSelectBed : evSelectChamber, bedOrChamber->index);
			toolButtons[slot]->SetIcon(isBed ? IconBed : IconChamber);
			toolButtons[slot]->SetIntVal(bedOrChamber->index);
			toolButtons[slot]->SetPrintText(count > 1);

			++slot;
		}
		return count;
	}

	void AllToolsSeen()
	{
		size_t slot = 0;
		size_t bedCount = 0;
		size_t chamberCount = 0;
		auto firstBed = OM::GetFirstBed();
		if (firstBed != nullptr)
		{
			bedCount = AddBedOrChamber(firstBed, slot);
		}
		OM::IterateToolsWhile([&slot](OM::Tool*& tool, size_t)
		{
			tool->slot = slot;
			const bool hasHeater = tool->heaters[0] != nullptr;
			const bool hasSpindle = tool->spindle != nullptr;
			const bool hasExtruder = tool->extruders.IsNonEmpty();
			if (slot < MaxSlots)
			{
				toolButtons[slot]->SetEvent(evSelectHead, tool->index);
				toolButtons[slot]->SetIntVal(tool->index);
				toolButtons[slot]->SetPrintText(true);
				toolButtons[slot]->SetIcon(hasSpindle ? IconSpindle : IconNozzle);
				mgr.Show(toolButtons[slot], true);

				mgr.Show(extrusionFactors[slot], hasExtruder);
				if (hasExtruder)
				{
					extrusionFactors[slot]->SetEvent(extrusionFactors[slot]->GetEvent(), (int) tool->extruders.LowestSetBit());
				}

				// Spindle takes precedence
				if (hasSpindle)
				{
					ManageCurrentActiveStandbyFields(slot, true, evAdjustActiveRPM, tool->spindle->index);
					++slot;
				}
				else if (hasHeater)
				{
					if (nvData.GetHeaterCombineType() == HeaterCombineType::notCombined)
					{
						tool->IterateHeaters([&slot, &tool](OM::ToolHeater*, size_t)
						{
							// only one heater per slot can be displayed
							if (slot >= MaxSlots)
							{
								return;
							}
							ManageCurrentActiveStandbyFields(
									slot,
									true,
									evAdjustToolActiveTemp, tool->index,
									evAdjustToolStandbyTemp, tool->index);
							++slot;
						});
					}
					else
					{
						ManageCurrentActiveStandbyFields(
								slot,
								true,
								evAdjustToolActiveTemp, tool->index,
								evAdjustToolStandbyTemp, tool->index);
						++slot;
					}
				}
				else
				{
					// Hides everything by default
					ManageCurrentActiveStandbyFields(slot);
					++slot;
				}
			}
			return slot < MaxSlots;
		});
		auto firstChamber = OM::GetFirstChamber();
		if (firstChamber != nullptr)
		{
			chamberCount = AddBedOrChamber(firstChamber, slot, false);
		}

		// Fill remaining space with additional beds
		if (slot < MaxSlots && bedCount > 1)
		{
			OM::IterateBedsWhile([&slot](OM::Bed*& bed, size_t) {
				AddBedOrChamber(bed, slot);
				return slot < MaxSlots;
			}, 1);
		}

		// Fill remaining space with additional chambers
		if (slot < MaxSlots && chamberCount > 1)
		{
			OM::IterateChambersWhile([&slot](OM::Chamber*& chamber, size_t) {
				AddBedOrChamber(chamber, slot, false);
				return slot < MaxSlots;
			}, 1);
		}

		numToolColsUsed = slot;
		for (size_t i = slot; i < MaxSlots; ++i)
		{
			mgr.Show(toolButtons[i], false);
			mgr.Show(currentTemps[i], false);
			mgr.Show(activeTemps[i], false);
			mgr.Show(standbyTemps[i], false);
			mgr.Show(extrusionFactors[i], false);
		}
		ResetToolAndHeaterStates();
		AdjustControlPageMacroButtons();
#if DISPLAY_X == 800
		if (tuneToolNumberButtons[0] != nullptr)
		{
			RefreshTuneToolRows();
		}
		if (controlToolHeaderCards[0] != nullptr)
		{
			RefreshControlToolsPage();
		}
		if (controlExtrudeToolCards[0] != nullptr)
		{
			SelectControlExtrudePageForActiveTool();
			RefreshControlExtrudeTools();
		}
#endif
	}

	void SetSpindleActive(size_t spindleIndex, int32_t activeRpm)
	{
		auto spindle = OM::GetOrCreateSpindle(spindleIndex);
		if (spindle == nullptr)
		{
			return;
		}
		spindle->active = abs(activeRpm);
		if (!GetFirmwareFeatures().IsBitSet(m568TempAndRPM))
		{
			if (activeRpm == 0)
			{
				spindle->state = OM::SpindleState::stopped;
			}
			else if (activeRpm > 0)
			{
				spindle->state = OM::SpindleState::forward;
			}
			else
			{
				spindle->state = OM::SpindleState::reverse;
			}
		}

		OM::IterateToolsWhile([spindle](OM::Tool*& tool, size_t) {
			if (tool->slot < MaxSlots && tool->spindle == spindle)
			{
				activeTemps[tool->slot]->SetValue(tool->spindle->active);
			}
			return tool->slot < MaxSlots;
		});
	}

	void UpdateSpindleCurrent(OM::Spindle* spindle)
	{
		OM::IterateToolsWhile([spindle](OM::Tool*& tool, size_t) {
			if (tool->slot < MaxSlots && tool->spindle == spindle)
			{
				const OM::SpindleState state = spindle->state;
				currentTemps[tool->slot]->SetValue(
						(state == OM::SpindleState::stopped)
							? 0
							: (state == OM::SpindleState::forward)
							  	  ? spindle->current
							  	  : -spindle->current);
			}
			return tool->slot < MaxSlots;
		});
	}

	void SetSpindleCurrent(size_t spindleIndex, int32_t current)
	{
		auto spindle = OM::GetOrCreateSpindle(spindleIndex);
		if (spindle == nullptr)
		{
			return;
		}
		spindle->current = abs(current);
		if (!GetFirmwareFeatures().IsBitSet(m568TempAndRPM))
		{
			if (current == 0)
			{
				spindle->state = OM::SpindleState::stopped;
			}
			else if (current > 0)
			{
				spindle->state = OM::SpindleState::forward;
			}
			else
			{
				spindle->state = OM::SpindleState::reverse;
			}
		}
		UpdateSpindleCurrent(spindle);
	}

	void SetSpindleLimit(size_t spindleIndex, uint32_t value, bool max)
	{
		OM::Spindle *spindle = OM::GetOrCreateSpindle(spindleIndex);
		if (spindle != nullptr)
		{
			if (max)
			{
				spindle->max = value;
			}
			else
			{
				spindle->min = value;
			}
		}
	}

	void SetSpindleState(size_t spindleIndex, OM::SpindleState state)
	{
		OM::Spindle* spindle = OM::GetOrCreateSpindle(spindleIndex);
		if (spindle == nullptr)
		{
			return;
		}
		const bool changed = spindle->state != state;
		spindle->state = state;
		if (changed)
		{
			UpdateSpindleCurrent(spindle);
		}
	}

	// This handles the old path where tools were assigned to spindles
	void SetSpindleTool(int8_t spindleNumber, int8_t toolIndex)
	{
		auto sp = OM::GetOrCreateSpindle(spindleNumber);
		if (sp == nullptr)
		{
			return;
		}
		if (toolIndex == -1)
		{
			OM::IterateToolsWhile([sp](OM::Tool*& tool, size_t) {
				if (tool->spindle == sp)
				{
					tool->spindle = nullptr;
				}
				return true;
			});
		}
		else
		{
			OM::Tool *tool = OM::GetOrCreateTool(toolIndex);
			if (tool != nullptr)
			{
				tool->spindle = sp;
			}
		}
	}

	void UpdateToolStatus(size_t toolIndex, OM::ToolStatus status)
	{
		auto tool = OM::GetTool(toolIndex);
		if (tool == nullptr)
		{
			return;
		}
		tool->status = status;
		Colour c = /*(status == OM::ToolStatus::standby) ? colours->standbyBackColour : */
					(status == OM::ToolStatus::active) ? colours->activeBackColour
					: colours->buttonImageBackColour;
		if (tool->slot < MaxSlots)
		{
			toolButtons[tool->slot]->SetColours(colours->buttonTextColour, c);
		}
#if DISPLAY_X == 800
		if (currentUiPage == UiPage::ControlTools) RefreshControlToolsPage();
#endif
	}

	void SetToolExtruder(size_t toolIndex, uint8_t extruder)
	{
		OM::Tool *tool = OM::GetOrCreateTool(toolIndex);
		if (tool != nullptr)
		{
			tool->extruders.SetBit(extruder);
#if DISPLAY_X == 800
			if (tuneToolNumberButtons[0] != nullptr)
			{
				RefreshTuneToolRows();
			}
			if (controlExtrudeToolCards[0] != nullptr)
			{
				RefreshControlExtrudeTools();
			}
#endif
		}
	}

	void SetToolFan(size_t toolIndex, uint8_t fan)
	{
		OM::Tool *tool = OM::GetOrCreateTool(toolIndex);
		if (tool != nullptr)
		{
			tool->fans.SetBit(fan);
#if DISPLAY_X == 800
			if (tuneToolNumberButtons[0] != nullptr)
			{
				RefreshTuneToolRows();
			}
#endif
		}
	}

	bool RemoveToolHeaters(const size_t toolIndex, const uint8_t firstIndexToDelete)
	{
		OM::Tool *tool = OM::GetOrCreateTool(toolIndex);
		if (tool == nullptr)
		{
			return false;
		}
		return tool->RemoveHeatersFrom(firstIndexToDelete) > 0;
	}

	void SetToolHeater(size_t toolIndex, uint8_t toolHeaterIndex, uint8_t heaterIndex)
	{
		OM::Tool *tool = OM::GetOrCreateTool(toolIndex);
		if (tool == nullptr)
		{
			return;
		}
		OM::ToolHeater *toolHeater = tool->GetOrCreateHeater(toolHeaterIndex);
		if (toolHeater == nullptr)
		{
			return;
		}
		toolHeater->heaterIndex = heaterIndex;
#if DISPLAY_X == 800
		if (controlExtrudeToolCards[0] != nullptr)
		{
			RefreshControlExtrudeTools();
		}
#endif
	}

	void SetToolOffset(size_t toolIndex, size_t axisIndex, float offset)
	{
		if (axisIndex < MaxTotalAxes)
		{
			OM::Tool *tool = OM::GetOrCreateTool(toolIndex);
			if (tool != nullptr)
			{
				tool->offsets[axisIndex] = offset;
			}
		}
	}

	// This handles the new path were spindles are assigned to tools
	void SetToolSpindle(int8_t toolIndex, int8_t spindleNumber)
	{
		// Old spindles[].tool is handled by SetSpindleTool
		OM::Tool *tool = OM::GetOrCreateTool(toolIndex);
		if (tool == nullptr)
		{
			return;
		}
		if (spindleNumber == -1)
		{
			tool->spindle = nullptr;
		}
		else
		{
			OM::Spindle* spindle = OM::GetSpindle(spindleNumber);
			if (spindle == nullptr)
			{
				return;
			}
			tool->spindle = spindle;
		}
	}

	void SetBabystepOffset(size_t index, float f)
	{
		if (index < MaxTotalAxes)
		{
			OM::Axis *axis = OM::GetOrCreateAxis(index);
			if (axis == nullptr)
			{
				return;
			}
			axis->babystep = f;
			// In first initialization we will see babystep before letter
			// so this won;t be true hence it is also set in UpdateGeometry
			if (axis->letter[0] == 'Z')
			{
				babystepOffsetField->SetValue(f);
#if DISPLAY_X == 800
				if (tuneZOffsetButton != nullptr)
				{
					tuneZOffsetText.printf("%.3f", (double)f);
					tuneZOffsetButton->SetText(tuneZOffsetText.c_str());
				}
#endif
			}
		}
	}

	void SetAxisLetter(size_t index, char l)
	{
		if (index < MaxTotalAxes)
		{
			OM::Axis *axis = OM::GetOrCreateAxis(index);
			if (axis != nullptr)
			{
				axis->letter[0] = l;
#if DISPLAY_X == 800
				if (l == 'X') statusObjectXAxis = static_cast<int>(index);
				else if (l == 'Y') statusObjectYAxis = static_cast<int>(index);
				statusObjectsDirty = true;
				RefreshControlMoveHoming();
#endif
			}
		}
	}

	void SetAxisVisible(size_t index, bool v)
	{
		if (index < MaxTotalAxes)
		{
			OM::Axis *axis = OM::GetOrCreateAxis(index);
			if (axis != nullptr)
			{
				axis->visible = v;
#if DISPLAY_X == 800
				RefreshControlMoveHoming();
#endif
			}
		}
	}

	void SetAxisWorkplaceOffset(size_t axisIndex, size_t workplaceIndex, float offset)
	{
		if (axisIndex < MaxTotalAxes && workplaceIndex < OM::Workplaces::MaxTotalWorkplaces)
		{
			OM::Axis *axis = OM::GetOrCreateAxis(axisIndex);
			if (axis != nullptr)
			{
				axis->workplaceOffsets[workplaceIndex] = offset;
			}
		}
	}

	void SetCurrentWorkplaceNumber(uint8_t workplaceNumber)
	{
		if (currentWorkplaceNumber == workplaceNumber || workplaceNumber >= OM::Workplaces::MaxTotalWorkplaces)
		{
			return;
		}
		currentWorkplaceNumber = workplaceNumber;
	}

	void SetBedOrChamberHeater(const uint8_t heaterIndex, const int8_t heaterNumber, bool bed)
	{
		if (bed)
		{
			auto bed = OM::GetOrCreateBed(heaterIndex);
			if (bed != nullptr)
			{
				bed->heater = heaterNumber;
			}
		}
		else
		{
			auto chamber = OM::GetOrCreateChamber(heaterIndex);
			if (chamber != nullptr)
			{
				chamber->heater = heaterNumber;
			}
		}
	}
}

// End
