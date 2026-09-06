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
static DisplayField *emptyRoot, *baseRoot, *commonRoot, *controlRoot, *printRoot, *messageRoot, *setupRoot;
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

// Helper to register fields into a group and show/hide groups
static void RegisterField(DisplayField *arr[], size_t &count, DisplayField *f)
{
	if (f != nullptr && count < 64)
	{
		arr[count++] = f;
	}
}

static void ShowGroup(DisplayField *arr[], size_t count, bool show)
{
	for (size_t i = 0; i < count; ++i)
	{
		mgr.Show(arr[i], show);
	}
}
// -- END: New fields and helpers

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
static SingleButton *tabControl, *tabStatus, *tabMsg, *tabSetup;
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

const size_t maxUserCommandLength = 40;                    // max length of a user gcode command
const size_t numUserCommandBuffers = 6;                    // number of command history buffers plus one

static String<maxUserCommandLength> userCommandBuffers[numUserCommandBuffers];
static size_t currentUserCommandBuffer = 0, currentHistoryBuffer = 0;

static unsigned int numToolColsUsed = 0;
static unsigned int numHeaterAndToolColumns = 0;
static int oldIntValue;
static Event eventToConfirm = evNull;
static uint8_t numVisibleAxes = 0;                        // initialise to 0 so we refresh the macros list when we receive the number of axes
static uint8_t numDisplayedAxes = 0;
static bool isDelta = false;

const char* _ecv_array null currentFile = nullptr;            // file whose info is displayed in the file info popup
const StringTable * strings = &LanguageTables[0];
static bool keyboardIsDisplayed = false;
static bool keyboardShifted = false;

int32_t alertMode = -1;                                    // the mode of the current alert, or -1 if no alert displayed
uint32_t alertTicks = 0;
uint32_t infoTimeout = DefaultInfoTimeout;                // info timeout in seconds, 0 means don't display into messages at all
uint32_t whenAlertReceived;
bool displayingResponse = false;                        // true if displaying a response

static PixelNumber screensaverTextWidth = 0;
static uint32_t lastScreensaverMoved = 0;

static uint8_t currentWorkplaceNumber = OM::MaxTotalWorkplaces;
static int8_t currentTool = -2;                            // Initialized to a value never returned by RRF to have the logic for "no tool" applied at startup
static bool allAxesHomed = false;
static const bool isLandscape = true;                     // Once portrait mode is enabled, this needs to be de-const-ed

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
IntegerButton *AddIntegerButton(PixelNumber row, unsigned int col, unsigned int numCols, const char * _ecv_array null label, const char * _ecv_array null units, Event evt, PixelNumber displayWidt[...]
