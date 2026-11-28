// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 KDEGlance contributors
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
// Based on KDEGlance's Dolphin hook (https://github.com/AlexGraveler/KDEGlance).
#include "dolphinaccess.h"
#include "dolphinsymbols.h"
#include "hookguard.h"

#include <KFileItem>

#include <QAbstractScrollArea>
#include <QCoreApplication>
#include <QGraphicsObject>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLineEdit>
#include <QLoggingCategory>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QWidget>

#include <dlfcn.h>

Q_LOGGING_CATEGORY(lcAccess, "kwiklook.dolphin.access", QtInfoMsg)

using namespace Qt::StringLiterals;

// Member functions are called through plain function pointers whose first
// parameter is `this`. With the Itanium C++ ABI (x86-64, AArch64, ...) this is
// exactly how non-virtual member calls are made, including the hidden return
// slot for class types such as KFileItem.
struct DolphinAccess::Api {
    using CountFn = int (*)(const void *);
    using FileItemFn = KFileItem (*)(const void *, int);
    using IndexOfUrlFn = int (*)(const void *, const QUrl &);
    using IntConstFn = int (*)(const void *);
    using IsSelectedFn = bool (*)(const void *, int);
    using BoolConstFn = bool (*)(const void *);
    using SetIntFn = void (*)(void *, int);
    using VoidFn = void (*)(void *);
    using SetSelectedFn = void (*)(void *, int, int, int);
    using ScrollPosFn = void (*)(void *, int, int);

    CountFn modelCount = nullptr;
    FileItemFn modelFileItem = nullptr;
    IndexOfUrlFn modelIndexOfUrl = nullptr;
    IntConstFn selCurrentItem = nullptr;
    IsSelectedFn selIsSelected = nullptr;
    BoolConstFn selHasSelection = nullptr;
    SetIntFn selSetCurrentItem = nullptr;
    VoidFn selClearSelection = nullptr;
    SetSelectedFn selSetSelected = nullptr;
    SetIntFn selBeginAnchored = nullptr;
    VoidFn selEndAnchored = nullptr;
    ScrollPosFn viewScrollToItemPos = nullptr; // scrollToItem(int, ViewItemPosition)
    SetIntFn viewScrollToItem = nullptr; // older scrollToItem(int)
    BoolConstFn ctrlTypeAhead = nullptr;
    BoolConstFn ctrlSelectionMode = nullptr;
};

namespace
{
// KItemListSelectionManager::SelectionMode::Select
constexpr int SelectModeSelect = 0;
// KItemListView::ViewItemPosition::Nearest
constexpr int ViewItemPositionNearest = 3;

void *lookup(const char *symbol, QStringList *missing)
{
    void *p = ::dlsym(RTLD_DEFAULT, symbol);
    if (!p) {
        missing->append(QString::fromLatin1(symbol));
    }
    return p;
}

QObject *childInheriting(const QObject *parent, const char *className)
{
    if (!parent) {
        return nullptr;
    }
    const auto children = parent->children();
    for (QObject *child : children) {
        if (child->inherits(className)) {
            return child;
        }
    }
    return nullptr;
}
} // namespace

const DolphinAccess &DolphinAccess::instance()
{
    static const DolphinAccess access;
    return access;
}

DolphinAccess::DolphinAccess()
    : m_api(new Api)
{
    resolve();
    if (!hasModelAccess()) {
        return; // the fallback calls nothing that could crash
    }
    m_guard = new HookGuard(HookGuard::defaultFile(), HookGuard::versionsKey(QCoreApplication::applicationVersion()));
    if (const QString broken = m_guard->broken(); !broken.isEmpty()) {
        m_check = Check::Failed;
        m_problem = broken;
        qCWarning(lcAccess) << "not using Dolphin's internals:" << broken;
    }
}

void DolphinAccess::resolve()
{
    namespace S = DolphinSymbols;
    // Optional ones: features degrade gracefully without them.
    QStringList optionalMissing;
    const auto find = [&](const S::Symbol &symbol) {
        return lookup(symbol.name, symbol.need == S::Need::Required ? &m_missing : &optionalMissing);
    };
    Api &a = *m_api;
    a.modelCount = reinterpret_cast<Api::CountFn>(find(S::ModelCount));
    a.modelFileItem = reinterpret_cast<Api::FileItemFn>(find(S::ModelFileItem));
    a.modelIndexOfUrl = reinterpret_cast<Api::IndexOfUrlFn>(find(S::ModelIndexOfUrl));
    a.selCurrentItem = reinterpret_cast<Api::IntConstFn>(find(S::SelCurrentItem));
    a.selIsSelected = reinterpret_cast<Api::IsSelectedFn>(find(S::SelIsSelected));
    a.selHasSelection = reinterpret_cast<Api::BoolConstFn>(find(S::SelHasSelection));
    a.selSetCurrentItem = reinterpret_cast<Api::SetIntFn>(find(S::SelSetCurrentItem));
    a.selClearSelection = reinterpret_cast<Api::VoidFn>(find(S::SelClearSelection));
    a.selSetSelected = reinterpret_cast<Api::SetSelectedFn>(find(S::SelSetSelected));
    a.selBeginAnchored = reinterpret_cast<Api::SetIntFn>(find(S::SelBeginAnchored));
    a.selEndAnchored = reinterpret_cast<Api::VoidFn>(find(S::SelEndAnchored));
    a.viewScrollToItemPos = reinterpret_cast<Api::ScrollPosFn>(find(S::ViewScrollToItemPos));
    // Only one of the two forms exists in a given Dolphin: don't report the other.
    a.viewScrollToItem = a.viewScrollToItemPos ? nullptr : reinterpret_cast<Api::SetIntFn>(find(S::ViewScrollToItem));
    a.ctrlTypeAhead = reinterpret_cast<Api::BoolConstFn>(find(S::CtrlTypeAhead));
    a.ctrlSelectionMode = reinterpret_cast<Api::BoolConstFn>(find(S::CtrlSelectionMode));

    if (!m_missing.isEmpty()) {
        qCWarning(lcAccess) << "Dolphin internals not found, using fallbacks:" << m_missing;
    }
    if (!optionalMissing.isEmpty()) {
        qCInfo(lcAccess) << "optional Dolphin internals not found:" << optionalMissing;
    }
}

bool DolphinAccess::hasModelAccess() const
{
    return m_check != Check::Failed && m_api->modelCount && m_api->modelFileItem && m_api->selCurrentItem && m_api->selIsSelected && m_api->selHasSelection;
}

bool DolphinAccess::hasSelectionAccess() const
{
    return hasModelAccess() && m_api->modelIndexOfUrl && m_api->selSetCurrentItem && m_api->selClearSelection && m_api->selSetSelected
        && m_api->selBeginAnchored && m_api->selEndAnchored;
}

QStringList DolphinAccess::missingSymbols() const
{
    return m_missing;
}

QString DolphinAccess::checkState() const
{
    switch (m_check) {
    case Check::Pending:
        return u"pending"_s;
    case Check::Ok:
        return u"ok"_s;
    case Check::Failed:
        break;
    }
    return u"failed"_s;
}

QString DolphinAccess::problem() const
{
    return m_problem;
}

bool DolphinAccess::ready(QWidget *view) const
{
    if (!hasModelAccess()) {
        return false;
    }
    if (m_check == Check::Pending) {
        runCheck(view);
    }
    return m_check != Check::Failed;
}

void DolphinAccess::runCheck(QWidget *view) const
{
    const QObject *model = modelOf(view);
    const QObject *selection = selectionManagerOf(view);
    if (!model || !selection) {
        return;
    }
    QString problem;
    const bool complete = m_guard->run(u"read"_s, [&] {
        bool done = false;
        problem = selfCheck(model, selection, controllerOf(view), &done);
        return done && problem.isEmpty();
    });
    if (!problem.isEmpty()) {
        // This process only: an odd answer may be a passing state, a crash is not.
        m_check = Check::Failed;
        m_problem = u"Dolphin's internals gave an unexpected answer: %1"_s.arg(problem);
        qCWarning(lcAccess) << "not using Dolphin's internals:" << m_problem;
    } else if (complete) {
        m_check = Check::Ok;
    }
}

// What the internals can be checked against without Dolphin's help: a changed
// return type gives garbage (or crashes, which HookGuard catches). Every function
// read on Space is called once here, under the guard. `complete` is false while
// the folder is empty (tried again on the next Space).
QString DolphinAccess::selfCheck(const QObject *model, const QObject *selection, const QObject *controller, bool *complete) const
{
    const Api &a = *m_api;
    *complete = false;
    const int count = a.modelCount(model);
    if (count < 0) {
        return u"%1 items"_s.arg(count);
    }
    const int current = a.selCurrentItem(selection);
    if (current < -1 || current > count) {
        return u"current item %1 of %2"_s.arg(current).arg(count);
    }
    a.selHasSelection(selection);
    if (controller && a.ctrlTypeAhead) {
        a.ctrlTypeAhead(controller);
    }
    if (controller && a.ctrlSelectionMode) {
        a.ctrlSelectionMode(controller);
    }
    if (count == 0) {
        return {};
    }
    const int i = current >= 0 && current < count ? current : 0;
    a.selIsSelected(selection, i);
    const KFileItem item = a.modelFileItem(model, i);
    if (item.isNull()) {
        return u"no item at %1 of %2"_s.arg(i).arg(count);
    }
    if (const int found = a.modelIndexOfUrl(model, item.url()); found != i) {
        return u"item %1 looked up again at %2"_s.arg(i).arg(found);
    }
    *complete = true;
    return {};
}

QWidget *DolphinAccess::viewFor(QWidget *widget)
{
    for (QWidget *w = widget; w; w = w->parentWidget()) {
        if (w->inherits("DolphinView")) {
            return w;
        }
        if (w->isWindow()) {
            break;
        }
    }
    return nullptr;
}

QWidget *DolphinAccess::containerOf(QWidget *view)
{
    return qobject_cast<QWidget *>(childInheriting(view, "KItemListContainer"));
}

QObject *DolphinAccess::controllerOf(QWidget *view)
{
    return childInheriting(containerOf(view), "KItemListController");
}

QObject *DolphinAccess::selectionManagerOf(QWidget *view)
{
    return childInheriting(controllerOf(view), "KItemListSelectionManager");
}

QObject *DolphinAccess::modelOf(QWidget *view)
{
    // KItemListController reparents its model in current Dolphin releases.
    // Older releases left it under DolphinView. Stay within this view's
    // controller so split panes never pick up each other's model.
    if (auto *model = childInheriting(controllerOf(view), "KFileItemModel")) {
        return model;
    }
    return childInheriting(view, "KFileItemModel");
}

QObject *DolphinAccess::itemListViewOf(QWidget *view)
{
    if (auto *listView = childInheriting(controllerOf(view), "KItemListView")) {
        return listView;
    }
    auto *container = qobject_cast<QAbstractScrollArea *>(containerOf(view));
    if (!container) {
        return nullptr;
    }
    auto *graphicsView = qobject_cast<QGraphicsView *>(container->viewport());
    if (!graphicsView || !graphicsView->scene()) {
        return nullptr;
    }
    const auto items = graphicsView->scene()->items();
    for (QGraphicsItem *item : items) {
        if (QGraphicsObject *object = item->toGraphicsObject(); object && object->inherits("KItemListView")) {
            return object;
        }
    }
    return nullptr;
}

bool DolphinAccess::isFileViewFocus(QWidget *focus, QWidget **viewOut)
{
    if (!focus) {
        return false;
    }
    // Text input anywhere (inline rename, filter bar, location bar) keeps Space.
    if (qobject_cast<QLineEdit *>(focus) || qobject_cast<QTextEdit *>(focus) || qobject_cast<QPlainTextEdit *>(focus)) {
        return false;
    }
    QWidget *view = viewFor(focus);
    if (!view) {
        return false;
    }
    QWidget *container = containerOf(view);
    if (!container || !(focus == container || container->isAncestorOf(focus))) {
        return false;
    }
    if (viewOut) {
        *viewOut = view;
    }
    return true;
}

int DolphinAccess::targetIndex(const QObject *model, const QObject *selection, const std::optional<QUrl> &preferred) const
{
    const Api &a = *m_api;
    const int count = a.modelCount(model);
    if (count <= 0) {
        return -1;
    }
    int target = -1;
    if (preferred) {
        if (a.modelIndexOfUrl) {
            target = a.modelIndexOfUrl(model, *preferred);
        }
        if (target < 0) {
            // The context menu may report the most-local URL; compare both.
            for (int i = 0; i < count && target < 0; ++i) {
                const KFileItem item = a.modelFileItem(model, i);
                if (item.url() == *preferred || item.mostLocalUrl() == *preferred) {
                    target = i;
                }
            }
        }
    } else {
        const int current = a.selCurrentItem(selection);
        const bool hasSelection = a.selHasSelection(selection);
        if (current >= 0 && current < count && (!hasSelection || a.selIsSelected(selection, current))) {
            target = current;
        } else if (hasSelection) {
            for (int i = 0; i < count; ++i) {
                if (a.selIsSelected(selection, i)) {
                    target = i;
                    break;
                }
            }
        }
    }
    if (target < 0 || target >= count) {
        return -1;
    }
    const KFileItem item = a.modelFileItem(model, target);
    if (item.isNull() || item.isDir()) {
        return -1; // folders keep Dolphin's own Space behaviour
    }
    return target;
}

DolphinAccess::Snapshot DolphinAccess::collect(const QObject *model, const QObject *selection, int target) const
{
    const Api &a = *m_api;
    const int count = a.modelCount(model);
    Snapshot s;
    // Two or more selected files: only those. The target is one of them (the
    // current item if selected, else the first selected).
    int selectedFiles = 0;
    if (a.selHasSelection(selection)) {
        for (int i = 0; i < count && selectedFiles < 2; ++i) {
            if (a.selIsSelected(selection, i)) {
                const KFileItem item = a.modelFileItem(model, i);
                selectedFiles += !item.isNull() && !item.isDir();
            }
        }
    }
    s.selectionOnly = selectedFiles >= 2 && a.selIsSelected(selection, target);
    s.urls.reserve(count);
    s.modelUrls.reserve(count);
    for (int i = 0; i < count; ++i) {
        const KFileItem item = a.modelFileItem(model, i);
        if (item.isNull() || item.isDir() || (s.selectionOnly && !a.selIsSelected(selection, i))) {
            continue;
        }
        if (i == target) {
            s.current = int(s.urls.size());
        }
        s.urls.append(item.mostLocalUrl());
        s.modelUrls.append(item.url());
    }
    return s;
}

std::optional<DolphinAccess::Snapshot> DolphinAccess::snapshot(QWidget *view, const std::optional<QUrl> &preferred) const
{
    if (!view || !ready(view)) {
        return std::nullopt;
    }
    const QObject *model = modelOf(view);
    const QObject *selection = selectionManagerOf(view);
    if (!model || !selection) {
        return std::nullopt;
    }
    const int target = targetIndex(model, selection, preferred);
    if (target < 0) {
        return std::nullopt;
    }
    Snapshot s = collect(model, selection, target);
    if (s.current < 0) {
        return std::nullopt;
    }
    return s;
}

bool DolphinAccess::hasFileTarget(QWidget *view) const
{
    if (!view || !ready(view)) {
        return false;
    }
    const QObject *model = modelOf(view);
    const QObject *selection = selectionManagerOf(view);
    return model && selection && targetIndex(model, selection, std::nullopt) >= 0;
}

bool DolphinAccess::isTypeAheadActive(QWidget *view, bool *known) const
{
    const QObject *controller = controllerOf(view);
    if (m_check == Check::Failed || (hasModelAccess() && !ready(view)) || !m_api->ctrlTypeAhead || !controller) {
        *known = false;
        return false;
    }
    *known = true;
    return m_api->ctrlTypeAhead(controller);
}

bool DolphinAccess::isSelectionModeActive(QWidget *view) const
{
    const QObject *controller = controllerOf(view);
    if (m_check == Check::Failed || (hasModelAccess() && !ready(view)) || !m_api->ctrlSelectionMode || !controller) {
        return false;
    }
    return m_api->ctrlSelectionMode(controller);
}

bool DolphinAccess::selectUrl(QWidget *view, const QUrl &modelUrl) const
{
    if (!hasSelectionAccess() || !view || !ready(view)) {
        return false;
    }
    QObject *model = modelOf(view);
    QObject *selection = selectionManagerOf(view);
    if (!model || !selection) {
        return false;
    }
    const Api &a = *m_api;
    const int index = a.modelIndexOfUrl(model, modelUrl);
    if (index < 0 || index >= a.modelCount(model)) {
        return false;
    }
    // The first time under these versions with a marker on disk (HookGuard).
    m_guard->run(u"select"_s, [&] {
        a.selEndAnchored(selection);
        a.selClearSelection(selection);
        a.selSetCurrentItem(selection, index);
        a.selSetSelected(selection, index, 1, SelectModeSelect);
        // Shift+click/arrow in Dolphin later extends from here.
        a.selBeginAnchored(selection, index);

        if (QObject *listView = itemListViewOf(view)) {
            if (a.viewScrollToItemPos) {
                a.viewScrollToItemPos(listView, index, ViewItemPositionNearest);
            } else if (a.viewScrollToItem) {
                a.viewScrollToItem(listView, index);
            }
        }
        return true;
    });
    return true;
}
