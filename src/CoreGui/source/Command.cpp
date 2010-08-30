/****************************************************************************
**
** Copyright (c) 2009-2010, Jaco Naude
**
** This file is part of Qtilities which is released under the following
** licensing options.
**
** Option 1: Open Source
** Under this license Qtilities is free software: you can
** redistribute it and/or modify it under the terms of the GNU General
** Public License as published by the Free Software Foundation, either
** version 3 of the License, or (at your option) any later version.
**
** Qtilities is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Qtilities. If not, see http://www.gnu.org/licenses/.
**
** Option 2: Commercial
** Alternatively, this library is also released under a commercial license
** that allows the development of closed source proprietary applications
** without restrictions on licensing. For more information on this option,
** please see the project website's licensing page:
** http://www.qtilities.org/licensing.html
**
** If you are unsure which license is appropriate for your use, please
** contact support@qtilities.org.
**
****************************************************************************/

#include "Command.h"
#include "QtilitiesApplication.h"
#include "QtilitiesCoreGuiConstants.h"

#include <Logger.h>

#include <QPointer>

using namespace Qtilities::CoreGui::Constants;

// --------------------------------
// Command Implemenation
// --------------------------------
Qtilities::CoreGui::Command::Command(QObject* parent) : QObject(parent)
{
    c = new CommandData;
}

Qtilities::CoreGui::Command::~Command() {
    delete c;
}

void Qtilities::CoreGui::Command::setDefaultKeySequence(const QKeySequence &key) {
    QKeySequence old_key_sequence = c->current_key_sequence;
    c->default_key_sequence = key;
    handleKeySequenceChange(old_key_sequence);
    emit keySequenceChanged();
}

void Qtilities::CoreGui::Command::setKeySequence(const QKeySequence &key) {
    QKeySequence old_key_sequence = c->current_key_sequence;
    c->current_key_sequence = key;
    handleKeySequenceChange(old_key_sequence);
    emit keySequenceChanged();
}

QKeySequence Qtilities::CoreGui::Command::defaultKeySequence() const {
    return c->default_key_sequence;
}

QKeySequence Qtilities::CoreGui::Command::keySequence() const {
    return c->current_key_sequence;
}

void Qtilities::CoreGui::Command::setDefaultText(const QString &text) {
    c->default_text = text;
}

QString Qtilities::CoreGui::Command::defaultText() const {
    return c->default_text;
}

// --------------------------------
// MultiContextAction Implemenation
// --------------------------------
struct Qtilities::CoreGui::MultiContextActionData {
    MultiContextActionData() : frontend_action(0),
    initialized(false),
    is_active(false) { }

    QAction* frontend_action;
    QString original_tooltip;
    bool initialized;
    bool is_active;
    QList<int> active_contexts;
    QPointer<QAction> active_backend_action;
    QHash<int, QPointer<QAction> > id_action_map;
};

Qtilities::CoreGui::MultiContextAction::MultiContextAction(QAction* user_visible_action, QObject* parent) : Command(parent) {
    d = new MultiContextActionData;

    d->frontend_action = user_visible_action;
    if (d->frontend_action) {
        d->frontend_action->setEnabled(false);
        d->frontend_action->setParent(this);
        d->original_tooltip = d->frontend_action->toolTip();
    }
}

Qtilities::CoreGui::MultiContextAction::~MultiContextAction() {
    delete d;
}

QAction* Qtilities::CoreGui::MultiContextAction::action() const {
    return d->frontend_action;
}

QShortcut* Qtilities::CoreGui::MultiContextAction::shortcut() const {
    return 0;
}

QString Qtilities::CoreGui::MultiContextAction::text() const
{
    if (!d->frontend_action)
        return QString();

    return d->frontend_action->text();
}

void Qtilities::CoreGui::MultiContextAction::addAction(QAction* action, QList<int> context_ids) {
    if (!action)
        return;

    if (context_ids.isEmpty()) {
        // Check if there is already an action for the standard context.
        if (d->id_action_map.keys().contains(0)) {
            LOG_WARNING(QString(tr("Attempting to register an action for a multi context (\"Standard Context\") action twice. Last action will be ignored: %1")).arg(action->text()));
            return;
        }

        // Add the action to the standard context (which will always be 0)
        d->id_action_map[0] = action;
    } else {
        for (int i = 0; i < context_ids.count(); i++) {
            // Check if there is already an action for this context
            if (d->id_action_map.keys().contains(context_ids.at(i))) {
                if (d->id_action_map[context_ids.at(i)] != 0) {
                    LOG_WARNING(tr("Attempting to register an action for a multi context action twice for a single context. Last action will be ignored: ") + action->text());
                    return;
                } else {
                    d->id_action_map[context_ids.at(i)] = action;
                }
            } else {
                d->id_action_map[context_ids.at(i)] = action;
            }
        }
    }

    updateFrontendAction();
}

bool Qtilities::CoreGui::MultiContextAction::isActive() {
    return d->is_active;
}

bool Qtilities::CoreGui::MultiContextAction::setCurrentContext(QList<int> context_ids) {
    #if defined(QTILITIES_VERBOSE_ACTION_DEBUGGING)
    LOG_TRACE("Context update request on command: " + defaultText());
    #endif

    // If this is just a place holder without any backend action we do nothing in here.
    if (d->id_action_map.count() == 0)
        return false;

    d->active_contexts = context_ids;
    QStringList contexts = CONTEXT_MANAGER->activeContextNames();
    if (d->active_backend_action) {
        QString action_text = d->active_backend_action->text();
        if (action_text == "Push Down")
            int i = 5;
    } else {
        QString action_text = text();
        if (action_text == "Push Down")
            int i = 5;
    }

    QAction *old_action = d->active_backend_action;
    d->active_backend_action = 0;
    for (int i = 0; i < d->active_contexts.size(); ++i) {
        if (QAction *a = d->id_action_map.value(d->active_contexts.at(i), 0)) {
            d->active_backend_action = a;

            #if defined(QTILITIES_VERBOSE_ACTION_DEBUGGING)
            LOG_TRACE("Backend action found: " + d->active_backend_action->text() + ", backend shortcut: " + d->active_backend_action->shortcut().toString() + ", MultiContextAction shortcut: " + d->frontend_action->shortcut().toString());
            #endif

            // This break will ensure that the first context is used for the case where multiple contexts are active at once.
            break;
        }
    }

    if (d->active_backend_action == old_action && d->initialized)  {
        #if defined(QTILITIES_VERBOSE_ACTION_DEBUGGING)
        LOG_TRACE("New backend action is the same as the current active backend action. Nothing to be done in here.");
        #endif
        if (d->active_backend_action) {
            QString action_text = d->active_backend_action->text();
            if (action_text == "Push Down")
                int i = 5;
        }
        return true;
    }

    // Disconnect signals from old action
    if (old_action) {
        disconnect(old_action, SIGNAL(changed()), this, SLOT(updateFrontendAction()));
        disconnect(d->frontend_action, SIGNAL(triggered(bool)), old_action, SIGNAL(triggered(bool)));
        disconnect(d->frontend_action, SIGNAL(toggled(bool)), old_action, SLOT(setChecked(bool)));
        #if defined(QTILITIES_VERBOSE_ACTION_DEBUGGING)
        QObject* parent = old_action->parent();
        QString parent_name = "Unspecified parent";
        if (parent) {
             parent_name = parent->objectName();
        }
        LOG_TRACE("Disconnecting multicontext action from previous backend action in parent: " + parent_name);
        #endif
    }

    // Connect signals for new action
    if (d->active_backend_action) {
        connect(d->active_backend_action, SIGNAL(changed()), this, SLOT(updateFrontendAction()));
        connect(d->frontend_action, SIGNAL(triggered(bool)), d->active_backend_action, SIGNAL(triggered(bool)));
        connect(d->frontend_action, SIGNAL(toggled(bool)), d->active_backend_action, SLOT(setChecked(bool)));
        updateFrontendAction();
        d->is_active = true;
        d->initialized = true;
        #if defined(QTILITIES_VERBOSE_ACTION_DEBUGGING)
        QObject* parent = d->active_backend_action->parent();
        QString parent_name = "Unspecified parent";
        if (parent) {
             parent_name = parent->objectName();
        }
        LOG_TRACE("Base action connected: " + d->active_backend_action->text() + ", Base shortcut: " + d->frontend_action->shortcut().toString() + ", Parent: " + parent_name);
        #endif
        return true;
    } else {
        #if defined(QTILITIES_VERBOSE_ACTION_DEBUGGING)
        LOG_TRACE("New backend action could not be found. Action will be disabled in this context.");
        #endif
    }
    // We can hide the action here if needed
    // d->frontend_action->setVisible(false);
    d->frontend_action->setEnabled(false);
    d->is_active = false;
    return false;
}

void Qtilities::CoreGui::MultiContextAction::updateFrontendAction() {
    // If this is just a place holder without any backend action we do nothing in here
    if (d->id_action_map.count() == 0)
        return;

    if (!d->active_backend_action || !d->frontend_action)
        return;

    // Update the icon
    d->frontend_action->setIcon(d->active_backend_action->icon());
    d->frontend_action->setIconText(d->active_backend_action->iconText());

    // Update the text
    d->frontend_action->setText(d->active_backend_action->text());
    d->frontend_action->setStatusTip(d->active_backend_action->statusTip());
    d->frontend_action->setWhatsThis(d->active_backend_action->whatsThis());

    d->frontend_action->setCheckable(d->active_backend_action->isCheckable());
    d->frontend_action->setEnabled(d->active_backend_action->isEnabled());
    d->frontend_action->setVisible(d->active_backend_action->isVisible());

    bool previous_block_value = d->frontend_action->blockSignals(true);
    d->frontend_action->setChecked(d->active_backend_action->isChecked());
    d->frontend_action->blockSignals(previous_block_value);
}

void Qtilities::CoreGui::MultiContextAction::handleKeySequenceChange(const QKeySequence& old_key) {
    // Check if the old key is part of the backend actions' tooltips:
    QString old_key_tooltip = QString("<span style=\"color: gray; font-size: small\">%2</span>").arg(old_key.toString(QKeySequence::NativeText));
    QAction* backend_action;
    for (int i = 0; i < d->id_action_map.count(); i++) {
        backend_action = d->id_action_map.values().at(i);
        if (backend_action->toolTip().endsWith(old_key_tooltip)) {
            QString chopped_tooltip = backend_action->toolTip();
            chopped_tooltip.chop(old_key_tooltip.size());
            backend_action->setToolTip(chopped_tooltip);
        }
    }

    // Check if there is no current action but a default. In that case we set the current to the default:
    if (c->current_key_sequence.isEmpty() && !c->default_key_sequence.isEmpty()) {
        c->current_key_sequence = c->default_key_sequence;
    }

    // Change the shortcut of the frontend action:
    d->frontend_action->setShortcut(c->current_key_sequence);

    // Add new key sequence to frontend action and all backend actions (backend actions only if there is a shortcut):
    QString new_key_tooltip = QString("<span style=\"color: gray; font-size: small\">%2</span>").arg(keySequence().toString(QKeySequence::NativeText));
    if (d->frontend_action->shortcut().isEmpty()) {
        if (d->active_backend_action)
            d->frontend_action->setToolTip(d->active_backend_action->toolTip());
        else
            d->frontend_action->setToolTip(d->original_tooltip);
    } else {
        if (d->active_backend_action)
            d->frontend_action->setToolTip(d->active_backend_action->toolTip() + " " + new_key_tooltip);
        else
            d->frontend_action->setToolTip(d->original_tooltip + " " + new_key_tooltip);

        // Add the new tooltip to all the backend actions' tooltips:
        for (int i = 0; i < d->id_action_map.count(); i++) {
            backend_action = d->id_action_map.values().at(i);
            backend_action->setToolTip(backend_action->toolTip() + " " + new_key_tooltip);
        }
    }
}

