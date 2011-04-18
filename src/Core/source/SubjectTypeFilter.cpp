/****************************************************************************
**
** Copyright (c) 2009-2011, Jaco Naude
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

#include "SubjectTypeFilter.h"
#include "QtilitiesCoreConstants.h"
#include "Observer.h"

#include <Logger.h>

#include <QMutex>
#include <QVariant>
#include <QDomElement>
#include <QDomDocument>

using namespace Qtilities::Core::Constants;

namespace Qtilities {
    namespace Core {
        FactoryItem<QObject, SubjectTypeFilter> SubjectTypeFilter::factory;
    }
}

struct Qtilities::Core::SubjectTypeFilterPrivateData {
    SubjectTypeFilterPrivateData() : inversed_filtering(false),
        is_modified(false) {}

    bool                    inversed_filtering;
    bool                    is_modified;
    QList<SubjectTypeInfo>  known_subject_types;
    QString                 known_objects_group_name;
};

Qtilities::Core::SubjectTypeFilter::SubjectTypeFilter(const QString& known_objects_group_name, QObject* parent) : AbstractSubjectFilter(parent) {
    d = new SubjectTypeFilterPrivateData;
    d->known_objects_group_name = known_objects_group_name;
}

Qtilities::Core::SubjectTypeFilter::~SubjectTypeFilter() {
    delete d;
}

void Qtilities::Core::SubjectTypeFilter::operator=(const SubjectTypeFilter& ref) {
    d->inversed_filtering = ref.inverseFilteringEnabled();
    d->known_subject_types = ref.knownSubjectTypes();
    d->known_objects_group_name = ref.groupName();
}

bool Qtilities::Core::SubjectTypeFilter::operator==(const SubjectTypeFilter& ref) const {
    if (d->inversed_filtering != ref.inverseFilteringEnabled())
        return false;
    if (d->known_subject_types != ref.knownSubjectTypes())
        return false;
    if (d->known_objects_group_name != ref.groupName())
        return false;

    return true;
}

bool Qtilities::Core::SubjectTypeFilter::operator!=(const SubjectTypeFilter& ref) const {
    return !(*this==ref);
}

bool Qtilities::Core::SubjectTypeFilter::initializeAttachment(QObject* obj, QString* rejectMsg, bool import_cycle) {
    Q_UNUSED(import_cycle)

    #ifndef QT_NO_DEBUG
    Q_ASSERT(observer != 0);
    #endif
    #ifdef QT_NO_DEBUG
    if (!obj) {
        if (rejectMsg)
            *rejectMsg = QString(tr("Subject Type Filter: Invalid object reference received. Attachment cannot be done."));
        return false;
    }
    #endif

    if (!observer) {
        if (rejectMsg)
            *rejectMsg = QString(tr("Subject Type Filter: Cannot evaluate an attachment in a subject filter without an observer context."));
        LOG_TRACE("Cannot evaluate an attachment in a subject filter without an observer context.");
        return false;
    }

    bool is_known_type = false;
    // If inversed and there are no known types is_known_type must be true:
    if (d->inversed_filtering && d->known_subject_types.count() == 0)
        is_known_type = true;

    // Check the obj meta info against the known filter types
    for (int i = 0; i < d->known_subject_types.count(); i++) {
        QString meta_type = d->known_subject_types.at(i).d_meta_type;
        if (obj->inherits(meta_type.toAscii().data())) {
            if (!d->inversed_filtering) {
                is_known_type = true;
                break;
            }
        } else {
            if (d->inversed_filtering) {
                is_known_type = true;
                break;
            }
        }
    }

    if (!is_known_type) {
        QString msg = QString(tr("Subject filter \"%1\" rejected attachment of object \"%2\" to observer \"%3\". It is not an allowed type in this context.")).arg(filterName()).arg(obj->objectName()).arg(observer->observerName());
        #ifndef QT_NO_DEBUG
        for (int t = 0; t < d->known_subject_types.count(); t++)
            LOG_TRACE("Allowed types: Meta type: " + d->known_subject_types.at(t).d_meta_type + ", Type description: " + d->known_subject_types.at(t).d_name);
        LOG_TRACE("Attachment type: " + QString(obj->metaObject()->className()));
        if (d->inversed_filtering)
            LOG_TRACE("Inversed filtering status: Enabled");
        else
            LOG_TRACE("Inversed filtering status: Disabled");
        #endif

        if (rejectMsg)
            *rejectMsg = msg;
        LOG_WARNING(msg);
    }

    return is_known_type;
}

bool Qtilities::Core::SubjectTypeFilter::handleMonitoredPropertyChange(QObject* obj, const char* property_name, QDynamicPropertyChangeEvent* propertyChangeEvent) {
    Q_UNUSED(obj)
    Q_UNUSED(property_name)
    Q_UNUSED(propertyChangeEvent)
    return false;
}

QString Qtilities::Core::SubjectTypeFilter::groupName() const {
    return d->known_objects_group_name;
}

void Qtilities::Core::SubjectTypeFilter::setGroupName(const QString& group_name) {
    if (d->known_objects_group_name != group_name) {
        d->known_objects_group_name = group_name;
        setModificationState(true);
    }
}

void Qtilities::Core::SubjectTypeFilter::addSubjectType(SubjectTypeInfo subject_type_info) {
    if (observer) {
        if (observer->subjectCount() > 0)
            return;
    }

    d->known_subject_types.append(subject_type_info);
    setModificationState(true);
}

bool Qtilities::Core::SubjectTypeFilter::isKnownType(const QString& meta_type) const {
    for (int i = 0; i < d->known_subject_types.count(); i++) {
        if (d->known_subject_types.at(i).d_meta_type == meta_type) {
            if (!d->inversed_filtering)
                return true;
        } else {
            if (d->inversed_filtering)
                return true;
        }
    }

    return false;
}

QList<Qtilities::Core::SubjectTypeInfo> Qtilities::Core::SubjectTypeFilter::knownSubjectTypes() const {
    return d->known_subject_types;
}

void Qtilities::Core::SubjectTypeFilter::enableInverseFiltering(bool enabled) {
    if (d->inversed_filtering != enabled) {
        if (observer) {
            if (observer->subjectCount() > 0)
                return;
        }

        d->inversed_filtering = enabled;
        setModificationState(true);
    }
}

bool Qtilities::Core::SubjectTypeFilter::inverseFilteringEnabled() const {
    return d->inversed_filtering;
}

Qtilities::Core::InstanceFactoryInfo Qtilities::Core::SubjectTypeFilter::instanceFactoryInfo() const {
    InstanceFactoryInfo instanceFactoryInfo(qti_def_FACTORY_QTILITIES,qti_def_FACTORY_TAG_SUBJECT_TYPE_FILTER,qti_def_FACTORY_TAG_SUBJECT_TYPE_FILTER);
    return instanceFactoryInfo;
}

Qtilities::Core::Interfaces::IExportable::ExportModeFlags Qtilities::Core::SubjectTypeFilter::supportedFormats() const {
    IExportable::ExportModeFlags flags = 0;
    flags |= IExportable::Binary;
    flags |= IExportable::XML;
    return flags;
}

Qtilities::Core::Interfaces::IExportable::Result Qtilities::Core::SubjectTypeFilter::exportBinary(QDataStream& stream) const {
    stream << d->inversed_filtering;
    stream << d->known_objects_group_name;
    stream << (quint32) d->known_subject_types.count();
    for (int i = 0; i < d->known_subject_types.count(); i++) {
        stream << d->known_subject_types.at(i).d_meta_type;
        stream << d->known_subject_types.at(i).d_name;
    }

    return IExportable::Complete;
}

Qtilities::Core::Interfaces::IExportable::Result Qtilities::Core::SubjectTypeFilter::importBinary(QDataStream& stream, QList<QPointer<QObject> >& import_list) {
    Q_UNUSED(import_list)

    stream >> d->inversed_filtering;
    stream >> d->known_objects_group_name;
    quint32 known_type_count;
    stream >> known_type_count;
    int known_type_count_int = known_type_count;
    d->known_subject_types.clear();
    for (int i = 0; i < known_type_count_int; i++) {
        QString meta_type;
        QString name;
        stream >> meta_type;
        stream >> name;
        SubjectTypeInfo new_type(meta_type, name);
        d->known_subject_types.append(new_type);
    }

    if (d->known_subject_types.count() == (int) known_type_count)
        return IExportable::Complete;
    else
        return IExportable::Failed;
}

Qtilities::Core::Interfaces::IExportable::Result Qtilities::Core::SubjectTypeFilter::exportXml(QDomDocument* doc, QDomElement* object_node) const {
    Q_UNUSED(doc)
    Q_UNUSED(object_node)
     
    if (d->inversed_filtering)
        object_node->setAttribute("InversedFiltering","True");
    if (!d->known_objects_group_name.isEmpty())
        object_node->setAttribute("GroupName",d->known_objects_group_name);
    object_node->setAttribute("TypeCount",d->known_subject_types.count());

    // Categories:
    if (d->known_subject_types.count() > 0) {
        QDomElement known_type_node = doc->createElement("KnownTypes");
        object_node->appendChild(known_type_node);
        for (int i = 0; i < d->known_subject_types.count(); i++) {
            QDomElement known_type = doc->createElement("Type_" + QString::number(i));
            known_type_node.appendChild(known_type);
            known_type.setAttribute("MetaType",d->known_subject_types.at(i).d_meta_type);
            known_type.setAttribute("Name",d->known_subject_types.at(i).d_name);
        }
    }

    return IExportable::Complete;
}

Qtilities::Core::Interfaces::IExportable::Result Qtilities::Core::SubjectTypeFilter::importXml(QDomDocument* doc, QDomElement* object_node, QList<QPointer<QObject> >& import_list) {
    Q_UNUSED(doc)
    Q_UNUSED(import_list)

    if (object_node->hasAttribute("InversedFiltering")) {
        if (object_node->attribute("InversedFiltering") == QString("True"))
            d->inversed_filtering = true;
        else
            d->inversed_filtering = false;
    }
    if (object_node->hasAttribute("GroupName"))
        d->known_objects_group_name = object_node->attribute("GroupName");

    int count_readback = 0;
    if (object_node->hasAttribute("TypeCount"))
        count_readback = object_node->attribute("TypeCount").toInt();

    // Known types stuff:
    QDomNodeList childNodes = object_node->childNodes();
    for(int i = 0; i < childNodes.count(); i++)
    {
        QDomNode childNode = childNodes.item(i);
        QDomElement child = childNode.toElement();

        if (child.isNull())
            continue;

        if (child.tagName() == "KnownTypes") {
            QDomNodeList knownTypesNodes = child.childNodes();
            for(int i = 0; i < knownTypesNodes.count(); i++)
            {
                QDomNode knownTypesNode = knownTypesNodes.item(i);
                QDomElement knownType = knownTypesNode.toElement();

                if (knownType.isNull())
                    continue;

                if (knownType.tagName().startsWith("Type")) {
                    QString meta_type = knownType.attribute("MetaType");
                    QString name = knownType.attribute("Name");
                    if (meta_type.isEmpty() || name.isEmpty()) {
                        LOG_ERROR(tr("Invalid subject type filter parameters detected. This filter will not be included in the parsed tree."));
                        return IExportable::Failed;
                    }
                    SubjectTypeInfo new_type(meta_type, name);
                    d->known_subject_types << new_type;
                    continue;
                }
            }
            continue;
        }
    }

    if (d->known_subject_types.count() == count_readback)
        return IExportable::Complete;
    else
        return IExportable::Failed;
}

bool Qtilities::Core::SubjectTypeFilter::isModified() const {
    return d->is_modified;
}

void Qtilities::Core::SubjectTypeFilter::setModificationState(bool new_state, IModificationNotifier::NotificationTargets notification_targets) {
    d->is_modified = new_state;
    if (notification_targets & IModificationNotifier::NotifyListeners) {
        emit modificationStateChanged(new_state);
    }
}

QDataStream & operator<< (QDataStream& stream, const Qtilities::Core::SubjectTypeFilter& stream_obj) {
    stream_obj.exportBinary(stream);
    return stream;
}

QDataStream & operator>> (QDataStream& stream, Qtilities::Core::SubjectTypeFilter& stream_obj) {
    QList<QPointer<QObject> > import_list;
    stream_obj.importBinary(stream,import_list);
    return stream;
}
