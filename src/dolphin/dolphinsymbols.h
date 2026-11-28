// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include <array>

/*
 * The libdolphinprivate functions the Dolphin hook calls (DolphinAccess), by
 * mangled name (Itanium C++ ABI). One list for the hook and for the test that
 * checks them against the installed Dolphin (tst_dolphinsymbols, run in CI).
 *
 * A mangled name covers the class, the function and its parameters, not the
 * return type: a changed return type still resolves.
 */
namespace DolphinSymbols
{
enum class Need {
    Required, // without it the hook uses its fallback (the selected file alone)
    Optional, // without it one detail degrades (scrolling, type-ahead detection)
    Older, // an older Dolphin's form of a function; `newer` names its successor
};

struct Symbol {
    const char *name;
    Need need;
    const char *newer = nullptr;
};

// KFileItemModel
inline constexpr Symbol ModelCount{"_ZNK14KFileItemModel5countEv", Need::Required};
inline constexpr Symbol ModelFileItem{"_ZNK14KFileItemModel8fileItemEi", Need::Required};
inline constexpr Symbol ModelIndexOfUrl{"_ZNK14KFileItemModel5indexERK4QUrl", Need::Required};
// KItemListSelectionManager
inline constexpr Symbol SelCurrentItem{"_ZNK25KItemListSelectionManager11currentItemEv", Need::Required};
inline constexpr Symbol SelIsSelected{"_ZNK25KItemListSelectionManager10isSelectedEi", Need::Required};
inline constexpr Symbol SelHasSelection{"_ZNK25KItemListSelectionManager12hasSelectionEv", Need::Required};
inline constexpr Symbol SelSetCurrentItem{"_ZN25KItemListSelectionManager14setCurrentItemEi", Need::Required};
inline constexpr Symbol SelClearSelection{"_ZN25KItemListSelectionManager14clearSelectionEv", Need::Required};
inline constexpr Symbol SelSetSelected{"_ZN25KItemListSelectionManager11setSelectedEiiNS_13SelectionModeE", Need::Required};
inline constexpr Symbol SelBeginAnchored{"_ZN25KItemListSelectionManager22beginAnchoredSelectionEi", Need::Required};
inline constexpr Symbol SelEndAnchored{"_ZN25KItemListSelectionManager20endAnchoredSelectionEv", Need::Required};
// KItemListView: scrollToItem(int, ViewItemPosition), before that scrollToItem(int)
inline constexpr Symbol ViewScrollToItemPos{"_ZN13KItemListView12scrollToItemEiNS_16ViewItemPositionE", Need::Optional};
inline constexpr Symbol ViewScrollToItem{"_ZN13KItemListView12scrollToItemEi", Need::Older, ViewScrollToItemPos.name};
// KItemListController
inline constexpr Symbol CtrlTypeAhead{"_ZNK19KItemListController23isSearchAsYouTypeActiveEv", Need::Optional};
inline constexpr Symbol CtrlSelectionMode{"_ZNK19KItemListController13selectionModeEv", Need::Optional};

inline constexpr std::array All{
    ModelCount,
    ModelFileItem,
    ModelIndexOfUrl,
    SelCurrentItem,
    SelIsSelected,
    SelHasSelection,
    SelSetCurrentItem,
    SelClearSelection,
    SelSetSelected,
    SelBeginAnchored,
    SelEndAnchored,
    ViewScrollToItemPos,
    ViewScrollToItem,
    CtrlTypeAhead,
    CtrlSelectionMode,
};
} // namespace DolphinSymbols
