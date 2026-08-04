AttributeTableArtefact __attrArtefactTable;

void ArtefactAttributeState_Link()
{
}

void AttributeTableArtefact::allocObjects(int objectQnty)
{
    m_table = new AttributeArtefact[objectQnty];
    if (m_table == NULL)
        m_maxObjectQnty = 0;
}

void AttributeTableArtefact::freeObjects()
{
    delete [] m_table;
    m_table = NULL;
    m_maxObjectQnty = 0;
}

ct_Object *AttributeTableArtefact::getObjectPTR(int index)
{
    s_ASSERT(index >= 0 && index < m_maxObjectQnty,
             "AttributeTableArtefact::getObjectPTR");
    return &(m_table[index]);
}

int AttributeArtefact::receiveEvent(KR_Event &event)
{
    return ct_Attribute::receiveEvent(event);
}

#define RR2NW_RGB_TO_LIST(col) ((col) >> 16), ((col) >> 8) & 255, (col) & 255

void AttributeArtefact::update(double ts)
{
    KR_ObjectID skinID = context->searchObject(m_skinName);
    s_ASSERT(!skinID.isNUL(), "AttributeArtefact::update");

    KR_Event event;
    event.timeStamp = ts;
    event.label = sk_EV_QUERY_MODEL_PTR;
    event.destination = skinID;
    context->sendEventNow(event);
    s_ASSERT(event.label == sk_EV_QUERY_MODEL_PTR_OK,
             "AttributeArtefact::update");
    event.data.open(EDO_READ)
              .get(&m_cacheSkin, sizeof(void *))
              .close();

    if (m_useRay)
        m_rayColor = GRTransparentColor(RR2NW_RGB_TO_LIST(m_rayRGB));
    if (m_useCorona)
    {
        m_coronaHText = g_loadSmoke(m_coronaName, NULL);
        m_coronaColor = GRTransparentColor(
            m_coronaRGB >> 16, (m_coronaRGB >> 8) & 255,
            m_coronaRGB & 255);
    }
    m_portalTable = g_arena.searchSeanceClassTable("Portal");
}

#undef RR2NW_RGB_TO_LIST

bool ArtefactAttributeState_IsKnown(const KR_ObjectID &objectID)
{
    return __attrArtefactTable.searchAttribute(objectID) != NULL;
}

bool ArtefactAttributeState_IsRetailDefault(const KR_ObjectID &objectID)
{
    AttributeArtefact *attr = static_cast<AttributeArtefact *>(
        __attrArtefactTable.searchAttribute(objectID));
    if (attr == NULL)
        return false;

    const double maxCoronaDifference =
        attr->m_maxCoronaR > 20.0
            ? attr->m_maxCoronaR - 20.0
            : 20.0 - attr->m_maxCoronaR;
    const double coronaDifference =
        attr->m_coronaR > 0.4
            ? attr->m_coronaR - 0.4
            : 0.4 - attr->m_coronaR;
    return strcmp(attr->m_skinName, "sk.Artefact.0") == 0 &&
           maxCoronaDifference <= 1.0e-6 &&
           coronaDifference <= 1.0e-6 &&
           attr->m_coronaRGB == 0xFF00FF && attr->m_coronaAlpha == 150;
}

bool ArtefactAttributeState_CachesUnresolved(SimulationContext *context)
{
    if (context == NULL || g_arena.getContext() != context ||
        !context->isExist("Artefact.Attr.0"))
        return false;
    AttributeArtefact *attr = static_cast<AttributeArtefact *>(
        __attrArtefactTable.searchAttribute(
            context->searchObject("Artefact.Attr.0")));
    return attr != NULL && attr->m_cacheSkin == NULL &&
           attr->m_rayColor == 0 && attr->m_coronaHText == NULL &&
           attr->m_coronaColor == 0 && attr->m_portalTable == ct_NULLID;
}

bool ArtefactAttributeState_ReferencesResolved(SimulationContext *context)
{
    if (context == NULL || g_arena.getContext() != context ||
        !context->isExist("Artefact.Attr.0"))
        return false;
    AttributeArtefact *attr = static_cast<AttributeArtefact *>(
        __attrArtefactTable.searchAttribute(
            context->searchObject("Artefact.Attr.0")));
    if (attr == NULL || attr->m_cacheSkin == NULL ||
        attr->m_portalTable !=
            g_arena.searchSeanceClassTable("Portal"))
        return false;
    return (!attr->m_useRay || attr->m_rayColor != 0) &&
           (!attr->m_useCorona || attr->m_coronaColor != 0);
}

bool ArtefactAttributeState_ResolveReferences(SimulationContext *context,
                                              double timeStamp)
{
    if (context == NULL || g_arena.getContext() != context ||
        !std::isfinite(timeStamp) ||
        !context->isExist("Artefact.Attr.0") ||
        !context->isExist("sk.Artefact.0") ||
        g_arena.searchSeanceClassTable("Portal") == ct_NULLID)
        return false;
    AttributeArtefact *attr = static_cast<AttributeArtefact *>(
        __attrArtefactTable.searchAttribute(
            context->searchObject("Artefact.Attr.0")));
    if (attr == NULL)
        return false;
    if (!ArtefactAttributeState_ReferencesResolved(context))
        attr->update(timeStamp);
    return ArtefactAttributeState_ReferencesResolved(context);
}
