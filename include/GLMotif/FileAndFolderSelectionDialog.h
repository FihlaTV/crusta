#ifndef _FileAndFolderSelectionDialog_H_
#define _FileAndFolderSelectionDialog_H_


/** Since the GLMotif::FileAndFolderSelectionDialog has all its methods private, code
    can't really be shared here through subclassing a specialization. Most of
    the code here is copied directly from FileAndFolderSelectionDialog with minor tweaks
    to support selection of folders */


#include <string>
#include <Misc/CallbackData.h>
#include <Misc/CallbackList.h>
#include <GLMotif/Button.h>
#include <GLMotif/ListBox.h>
#include <GLMotif/DropdownBox.h>
#include <GLMotif/PopupWindow.h>

/* Forward declarations: */
namespace Comm {
class MulticastPipe;
}
namespace GLMotif {
class RowColumn;
class ScrolledListBox;
}


namespace GLMotif {


class FileAndFolderSelectionDialog : public PopupWindow
{
    /* Embedded classes: */
    public:
    class CallbackData:public Misc::CallbackData // Base class for file selection dialog callbacks
        {
        /* Elements: */
        public:
        FileAndFolderSelectionDialog* fileSelectionDialog; // Pointer to the file selection dialog that caused the event

        /* Constructors and destructors: */
        CallbackData(FileAndFolderSelectionDialog* sFileAndFolderSelectionDialog)
            :fileSelectionDialog(sFileAndFolderSelectionDialog)
            {
            }
        };

    class OKCallbackData:public CallbackData // Callback data when the OK button was clicked, or a file name was double-clicked
        {
        /* Elements: */
        public:
        std::string selectedFileName; // Fully qualified name of selected file

        /* Constructors and destructors: */
        OKCallbackData(FileAndFolderSelectionDialog* sFileAndFolderSelectionDialog,std::string sSelectedFileName)
            :CallbackData(sFileAndFolderSelectionDialog),
             selectedFileName(sSelectedFileName)
            {
            }
        };

    class CancelCallbackData:public CallbackData // Callback data when the cancel button was clicked
        {
        /* Constructors and destructors: */
        public:
        CancelCallbackData(FileAndFolderSelectionDialog* sFileAndFolderSelectionDialog)
            :CallbackData(sFileAndFolderSelectionDialog)
            {
            }
        };

    /* Elements: */
    private:
    Comm::MulticastPipe* pipe; // A multicast pipe to synchronize instances of the file selection dialog across a cluster; file selection dialog takes over ownership from caller
    const char* fileNameFilters; // Current filter expression for file names; semicolon-separated list of allowed extensions
    RowColumn* pathButtonBox; // Box containing the path component buttons
    int selectedPathButton; // Index of the currently selected path button; determines the displayed directory
    ScrolledListBox* fileList; // Scrolled list box containing all directories and matching files in the current directory
    DropdownBox* filterList; // Drop down box containing the selectable file name filters
    Button* enterButton; // Activates opening the selected directory
    Button* pickButton; // Activates picking the selected entry
    Misc::CallbackList okCallbacks; // Callbacks to be called when the OK button is selected, or a file name is double-clicked
    Misc::CallbackList cancelCallbacks; // Callbacks to be called when the cancel button is selected

    /* Private methods: */
    std::string getCurrentPath(void) const; // Constructs the full path name of the currently displayed directory
    bool readDirectory(void); // Reads all directories and files from the selected directory into the list box
    void updateActionButtons(void); // Removes or adds the action buttons from the dialog based on the current selection
    void setSelectedPathButton(int newSelectedPathButton); // Changes the selected path button
    void pathButtonSelectedCallback(Button::SelectCallbackData* cbData); // Callback called when one of the path buttons is selected
    void listItemValueChangedCallback(Misc::CallbackData* cbData); // Callback when a list item gets highlighted
    void listItemSelectedCallback(ListBox::ItemSelectedCallbackData* cbData); // Callback when a list item gets double-clicked
    void filterListValueChangedCallback(DropdownBox::ValueChangedCallbackData* cbData); // Callback when the selected file name filter changes
    void pickButtonSelectedCallback(Misc::CallbackData* cbData); // Callback called when the Pick button is pressed
    void enterButtonSelectedCallback(Misc::CallbackData* cbData); // Callback called when the Enter button is pressed
    void cancelButtonSelectedCallback(Misc::CallbackData* cbData); // Callback called when the Cancel button is pressed

    /* Constructors and destructors: */
    public:
    FileAndFolderSelectionDialog(WidgetManager* widgetManager,const char* titleString,const char* initialDirectory,const char* sFileNameFilters,Comm::MulticastPipe* sPipe =0); // Creates a file selection dialog with the given title, initial directory, and file name filter; starts from current directory if initialDirectory is 0
    virtual ~FileAndFolderSelectionDialog(void);

    /* Methods: */
    void addFileNameFilters(const char* newFileNameFilters); // Adds another extension list to the list of selectable filters
    Misc::CallbackList& getOKCallbacks(void) // Returns the list of OK callbacks
        {
        return okCallbacks;
        }
    Misc::CallbackList& getCancelCallbacks(void) // Returns the list of cancel callbacks
        {
        return cancelCallbacks;
        }
    void defaultCloseCallback(CallbackData* cbData); // Default callback function that simply deletes the dialog
};


} //end namespace GLMotif


#endif //_FileAndFolderSelectionDialog_H_
