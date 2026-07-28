AttributeSpark __defaultSparkAttr;
AttributeTableSpark __attrSparkTable;

void SparkAttributeState_Link()
{
}

void AttributeSpark::update(double ts)
{
    KR_Event event;

    KR_ObjectID spr = context->searchObject(m_skin);
    s_ASSERT1(!spr.isNUL(),
              "AttributeSpark::update. Unknown texture object %s", m_skin);

    event.label = sk_EV_QUERY_MODEL_PTR;
    event.timeStamp = ts;
    event.source = getObjectID();
    event.destination = spr;
    context->sendEventNow(event);
    s_ASSERT(event.label == sk_EV_QUERY_MODEL_PTR_OK,
             "AttributeSpark::update");
    event.data.open(EDO_READ)
              .get(&m_cacheSkin, sizeof(void *))
              .close();
}

void AttributeTableSpark::allocObjects(int objectQnty)
{
    m_table = new (std::nothrow) AttributeSpark[objectQnty];

    if (m_table == NULL)
        m_maxObjectQnty = 0;
}

void AttributeTableSpark::freeObjects()
{
    delete [] m_table;
    m_table = NULL;
    m_maxObjectQnty = 0;
}

ct_Object *AttributeTableSpark::getObjectPTR(int index)
{
    s_ASSERT(index >= 0 && index < m_maxObjectQnty,
             "AttributeTable::getObjectPTR");
    return &(m_table[index]);
}

int AttributeSpark::receiveEvent(KR_Event &event)
{
    if (!ct_Attribute::receiveEvent(event))
    switch (event.label)
    {
    case sp_EV_SET_PHASE_COUNT:
        {
            int count;
            event.data.open(EDO_READ)
                      .getInt(count)
                      .close();
            s_ASSERT(count <= AttributeSpark::MAX_PHASE && count >= 0,
                     "Spark::receiveEvent::sp_EV_SET_PHASE_COUNT");
            m_phaseCnt = count;
        }
        break;

    case sp_EV_SET_PHASE:
        {
            int u0, v0, u1, v1, num;
            double time;
            int brightness;
            int color;
            double radius;
            event.data.open(EDO_READ)
                      .getInt(num)
                      .descend(RECT2D_I, 0)
                        .getInt(u0)
                        .getInt(v0)
                        .getInt(u1)
                        .getInt(v1)
                      .ascend()
                      .getDouble(time)
                      .getInt(brightness)
                      .getInt(color)
                      .getDouble(radius)
                      .close();
            s_ASSERT(num >= 0 && num < m_phaseCnt,
                     "Spark::receiveEvent::sp_EV_SET_PHASE");
            m_phase[num].init(u0, v0, u1, v1, time,
                              brightness, color, radius);
        }
        break;
    default:
        return 0;
    }
    return 1;
}

bool SparkAttributeState_IsRetailFlash(const KR_ObjectID &objectID)
{
    AttributeSpark *attr = static_cast<AttributeSpark *>(
        __attrSparkTable.searchAttribute(objectID));
    if (attr == NULL)
        return false;
    if (strcmp(attr->m_skin, "sk.Fusion.0") != 0 || attr->m_phaseCnt != 6 ||
        !std::isfinite(attr->m_maxRadius) || attr->m_maxRadius <= 0.0)
        return false;

    static const SparkPhase expected[6] = {
        {5, 5, 45, 85, 0.04, 100, 3, 7},
        {58, 6, 98, 86, 0.03, 200, 3, 14},
        {106, 7, 146, 87, 0.03, 100, 3, 10},
        {160, 4, 200, 84, 0.1, 20, 3, 7},
        {210, 4, 250, 84, 0.1, 0, 3, 4},
        {210, 4, 250, 84, 0.0, 0, 3, 0}
    };

    for (int i = 0; i < 6; ++i)
    {
        const SparkPhase &actual = attr->m_phase[i];
        const SparkPhase &wanted = expected[i];
        const double timeDifference =
            actual.time > wanted.time ? actual.time - wanted.time
                                      : wanted.time - actual.time;
        const double radiusDifference =
            actual.radius > wanted.radius ? actual.radius - wanted.radius
                                          : wanted.radius - actual.radius;
        if (actual.u0 != wanted.u0 || actual.v0 != wanted.v0 ||
            actual.u1 != wanted.u1 || actual.v1 != wanted.v1 ||
            timeDifference > 1.0e-6 ||
            actual.brightness != wanted.brightness ||
            actual.color != wanted.color || radiusDifference > 1.0e-6)
            return false;
    }
    return true;
}

bool SparkAttributeState_ResolveEncodedIndex(
    SimulationContext *context, int encodedIndex,
    AttributeSpark **attribute)
{
    if (attribute == NULL)
        return false;
    *attribute = NULL;
    if (context == NULL || g_arena.getContext() != context ||
        encodedIndex == -1)
        return false;
    const ct_ClassTableID table =
        g_arena.searchSeanceClassTable("SparkAttr");
    KR_ObjectID object = context->searchObject("Spark.Flash");
    AttributeSpark *candidate = object.isNUL()
        ? NULL
        : static_cast<AttributeSpark *>(
              __attrSparkTable.searchAttribute(object));
    if (table == ct_NULLID || candidate == NULL ||
        g_arena.getAttributeIndex(table, object) != encodedIndex ||
        !SparkAttributeState_IsRetailFlash(object))
        return false;
    *attribute = candidate;
    return true;
}

bool SparkAttributeState_ResolveVisualResources(SimulationContext *context)
{
    if (SparkAttributeState_VisualResourcesResolved(context))
        return true;
    if (context == NULL || g_arena.getContext() != context)
        return false;
    KR_ObjectID object = context->searchObject("Spark.Flash");
    AttributeSpark *attribute = object.isNUL()
        ? NULL
        : static_cast<AttributeSpark *>(
              __attrSparkTable.searchAttribute(object));
    KR_ObjectID spriteID = KR_ObjectID::NUL();
    CViewTexture *texture = NULL;
    if (attribute == NULL || attribute->m_cacheSkin != NULL ||
        !SparkAttributeState_IsRetailFlash(object) ||
        !SkinResourceState_ResolveLoadedSprite(
            context, attribute->m_skin, &spriteID, &texture) ||
        spriteID.isNUL() || texture == NULL || texture->HImage() == NULL ||
        texture->Width() <= 0 || texture->Height() <= 0)
        return false;
    for (int index = 0; index < attribute->m_phaseCnt; ++index)
    {
        const SparkPhase &phase = attribute->m_phase[index];
        if (phase.u0 < 0 || phase.v0 < 0 || phase.u1 <= phase.u0 ||
            phase.v1 <= phase.v0 || phase.u1 > texture->Width() ||
            phase.v1 > texture->Height() ||
            !std::isfinite(phase.time) || phase.time < 0.0 ||
            (index + 1 < attribute->m_phaseCnt && phase.time <= 0.0) ||
            phase.brightness < 0 || phase.color < 0 ||
            !std::isfinite(phase.radius) || phase.radius < 0.0)
            return false;
    }
    attribute->m_cacheSkin = texture;
    return true;
}

bool SparkAttributeState_VisualResourcesResolved(
    SimulationContext *context)
{
    if (context == NULL || g_arena.getContext() != context)
        return false;
    KR_ObjectID object = context->searchObject("Spark.Flash");
    AttributeSpark *attribute = object.isNUL()
        ? NULL
        : static_cast<AttributeSpark *>(
              __attrSparkTable.searchAttribute(object));
    KR_ObjectID spriteID = KR_ObjectID::NUL();
    CViewTexture *texture = NULL;
    return attribute != NULL && attribute->m_cacheSkin != NULL &&
           SkinResourceState_ResolveLoadedSprite(
               context, attribute->m_skin, &spriteID, &texture) &&
           !spriteID.isNUL() && texture == attribute->m_cacheSkin &&
           texture->HImage() != NULL && texture->Width() > 0 &&
           texture->Height() > 0;
}

void SparkAttributeState_ClearVisualResources(SimulationContext *context)
{
    if (context == NULL)
        return;
    KR_ObjectID object = context->searchObject("Spark.Flash");
    AttributeSpark *attribute = object.isNUL()
        ? NULL
        : static_cast<AttributeSpark *>(
              __attrSparkTable.searchAttribute(object));
    if (attribute != NULL)
        attribute->m_cacheSkin = NULL;
}

unsigned long long SparkAttributeState_VisualResourceFingerprint(
    SimulationContext *context)
{
    if (!SparkAttributeState_VisualResourcesResolved(context))
        return 0;
    KR_ObjectID object = context->searchObject("Spark.Flash");
    AttributeSpark *attribute = static_cast<AttributeSpark *>(
        __attrSparkTable.searchAttribute(object));
    const unsigned long long offset = 14695981039346656037ull;
    const unsigned long long prime = 1099511628211ull;
    unsigned long long hash = offset;
    const char *name = "Spark.Flash";
    for (const unsigned char *byte =
             reinterpret_cast<const unsigned char *>(name);
         *byte != 0; ++byte)
    {
        hash ^= *byte;
        hash *= prime;
    }
    hash ^= 0;
    hash *= prime;
    for (const unsigned char *byte =
             reinterpret_cast<const unsigned char *>(attribute->m_skin);
         *byte != 0; ++byte)
    {
        hash ^= *byte;
        hash *= prime;
    }
    hash ^= 0;
    hash *= prime;
    const int dimensions[2] = {
        attribute->m_cacheSkin->Width(), attribute->m_cacheSkin->Height()};
    const unsigned char *bytes =
        reinterpret_cast<const unsigned char *>(dimensions);
    for (int index = 0; index < static_cast<int>(sizeof(dimensions)); ++index)
    {
        hash ^= bytes[index];
        hash *= prime;
    }
    return hash;
}
