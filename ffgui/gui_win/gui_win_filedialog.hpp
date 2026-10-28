/* ****************************************************************************************************************** */
// flipflip's gui (ffgui)
//
// Copyright (c) Philippe Kehl (flipflip at oinkzwurgl dot org)
// https://oinkzwurgl.org/projaeggd/ffxx/
//
// This program is free software: you can redistribute it and/or modify it under the terms of the
// GNU General Public License as published by the Free Software Foundation, either version 3 of the
// License.
//
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
// even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with this program.
// If not, see <https://www.gnu.org/licenses/>.

#ifndef __GUI_WIN_FILEDIALOG_HPP__
#define __GUI_WIN_FILEDIALOG_HPP__

//
#include "ffgui_inc.hpp"
//
#include <filesystem>
#include <regex>
//
#include "gui_widget_filter.hpp"
#include "gui_win.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWinFileDialog : public GuiWin
{
   public:
    enum Mode_e
    {
        FILE_OPEN,
        FILE_SAVE
    };

    GuiWinFileDialog(const std::string& name);
    virtual ~GuiWinFileDialog();

    // Setup dialog
    void InitDialog(const Mode_e mode);

    // Set current directory
    void SetDirectory(const std::string& directory);

    // Set (propose) filename for mode = FILE_SAVE
    void SetFilename(const std::string& fileName);

    // Set confirm dialog, default is true for mode = FILE_SAVE, false for mode = FILE_OPEN
    void SetConfirmSelect(const bool confirm);

    // Set confirm dialog, for mode = FILE_SAVE if file exists, default true
    void SetConfirmOverwrite(const bool confirm);

    // Set file filter
    void SetFileFilter(const std::string regex, const bool highlightOnly = false);

    // True if dialog is inited and we should continue calling DrawDialog()
    bool IsInit();

    // Draw dialog, return true when done, in which case result is the path (on success) or empty (on cancel)
    bool DrawDialog();

    // Get result once done, empty string on cancel
    std::string& GetPath();

   private:
    std::string name_;
    struct Config
    {
        std::string lastDir;
        HistoryEntries history;
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, lastDir, history)
    Config cfg_;

    struct DirEntry
    {
        DirEntry(const std::filesystem::directory_entry& entry);
        // clang-format off
            std::string           fullPath;   // "/full/path/to/file_or_dir"
            std::string           fileType;   // type icon
            std::string           fileName;   // "file_or_dir"
            std::string           fileDate;   // date string
            std::string           fileSize;   // size string
            float                 fileSizeWidth;
            float                 fileSizeLeftPad;
            std::string           linkTarget; // "/path/to/link/target/file_or_dir", or "" if not a symlink

            bool                  isFile;
            bool                  isDir;
            bool                  isHidden;

            std::uintmax_t        sortSize;
            int                   sortType;
            std::time_t           sortDate;

            bool                  highlight;
        // clang-format on
    };

    Mode_e dialogMode_;

    enum State_e
    {
        UNINIT,
        SELECT,
        CONFIRM,
        DONE
    };

    enum State_e dialogState_;

    std::string _currentDir;
    std::vector<std::string> currentDirParts_;
    std::vector<DirEntry> _currentDirEntries_;
    bool currentDirEntriesDirty_;

    bool showHidden_;
    bool dirsFirst_;
    std::string currentDirInput_;
    bool editCurrentDir_;

    bool confirmSelect_;
    bool confirmOverwrite_;
    GuiWidgetFilter fileFilter_;
    std::string confirmWinName_;

    std::string selectedFileName_;
    bool selectedFileValid_;
    std::string selectedPath_;

    std::string resultPath_;

    enum Order_e
    {
        ASC,
        DESC
    };
    enum Col_e
    {
        TYPE,
        NAME,
        SIZE,
        DATE
    };
    struct SortInfo
    {
        SortInfo(const Col_e _col, const Order_e _order) : col{ _col }, order{ _order }
        {
        }
        Col_e col;
        Order_e order;
    };
    std::vector<SortInfo> sortInfo_;

    void _Done();
    bool _DrawDialog();
    void _Check();

    void _ChangeDir(const std::string& path = "");
    void _RefreshDir();
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIN_FILEDIALOG_HPP__
