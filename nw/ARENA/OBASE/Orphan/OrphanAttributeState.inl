AttributeTableOrphan __attrOrphanTable;

void OrphanAttributeState_Link()
{
}

void AttributeTableOrphan::allocObjects(int objectQnty)
{
    m_table = new AttributeOrphan[objectQnty];
    if (m_table == NULL)
        m_maxObjectQnty = 0;
}

void AttributeTableOrphan::freeObjects()
{
    delete [] m_table;
    m_table = NULL;
    m_maxObjectQnty = 0;
}

ct_Object *AttributeTableOrphan::getObjectPTR(int index)
{
    s_ASSERT(index >= 0 && index < m_maxObjectQnty,
             "AttributeTableOrphan::getObjectPTR");
    return &(m_table[index]);
}

void AttributeOrphan::update(double)
{
    m_cacheExplosionTable =
        g_arena.searchSeanceClassTable("Explosion");
    m_cacheExplAttr = g_arena.getAttributeIndex(
        g_arena.searchSeanceClassTable("ExplosionAttr"),
        context->searchObject(m_explAttrName));
    m_smokeTableID = g_arena.searchSeanceClassTable("Smoke");
    m_smokeAttrID = context->searchObject(m_smokeAttrName);
}

bool OrphanAttributeState_IsRetailDefault(const KR_ObjectID &objectID)
{
    AttributeOrphan *attr = static_cast<AttributeOrphan *>(
        __attrOrphanTable.searchAttribute(objectID));
    if (attr == NULL)
        return false;

    return strcmp(attr->m_explAttrName, "Expl.Attr.Default") == 0 &&
           attr->m_deltaT == 0.2 && attr->m_collisionT == 0.1 &&
           attr->m_explosionTime == 0.5 && attr->m_minSpeed == 1.0 &&
           strcmp(attr->m_smokeAttrName, "Smoke.Attr.Small") == 0 &&
           attr->m_smokeDamage == 0.5 &&
           attr->m_cacheExplosionTable == ct_NULLID &&
           attr->m_cacheExplAttr == ct_NULLID &&
           attr->m_smokeTableID == ct_NULLID &&
           attr->m_smokeAttrID.isNUL();
}

namespace {

const unsigned long long kOrphanReferenceHashOffset =
    1469598103934665603ull;
const unsigned long long kOrphanReferenceHashPrime = 1099511628211ull;

void OrphanReferenceHashBytes(unsigned long long &hash,
                              const void *data, int size)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    for (int index = 0; index < size; ++index)
    {
        hash ^= bytes[index];
        hash *= kOrphanReferenceHashPrime;
    }
}

void OrphanReferenceHashString(unsigned long long &hash,
                               const char *value)
{
    if (value == NULL)
        value = "";
    OrphanReferenceHashBytes(hash, value,
                             static_cast<int>(strlen(value)) + 1);
}

AttributeOrphan *OrphanDefaultAttribute(SimulationContext *context)
{
    if (context == NULL || g_arena.getContext() != context)
        return NULL;
    KR_ObjectID object = context->searchObject("Orphan.Attr.Default");
    return object.isNUL() ? NULL : static_cast<AttributeOrphan *>(
        __attrOrphanTable.searchAttribute(object));
}

bool ResolveOrphanReferences(SimulationContext *context,
                             AttributeOrphan *attribute,
                             int *explosionTable, int *explosionAttribute,
                             ct_ClassTableID *smokeTable,
                             KR_ObjectID *smokeAttribute)
{
    if (context == NULL || attribute == NULL || explosionTable == NULL ||
        explosionAttribute == NULL || smokeTable == NULL ||
        smokeAttribute == NULL)
        return false;
    *explosionTable = g_arena.searchSeanceClassTable("Explosion");
    const ct_ClassTableID explosionAttributes =
        g_arena.searchSeanceClassTable("ExplosionAttr");
    KR_ObjectID explosionObject =
        context->searchObject(attribute->m_explAttrName);
    *explosionAttribute = explosionAttributes == ct_NULLID ||
            explosionObject.isNUL()
        ? ct_NULLID
        : g_arena.getAttributeIndex(explosionAttributes, explosionObject);
    *smokeTable = g_arena.searchSeanceClassTable("Smoke");
    *smokeAttribute = context->searchObject(attribute->m_smokeAttrName);
    return *explosionTable != ct_NULLID &&
           *explosionAttribute != ct_NULLID &&
           *smokeTable != ct_NULLID && !smokeAttribute->isNUL() &&
           __attrSmokeTable.searchAttribute(*smokeAttribute) != NULL;
}

}  // namespace

bool OrphanAttributeState_CachesUnresolved(SimulationContext *context)
{
    AttributeOrphan *attribute = OrphanDefaultAttribute(context);
    return attribute != NULL &&
           attribute->m_cacheExplosionTable == ct_NULLID &&
           attribute->m_cacheExplAttr == ct_NULLID &&
           attribute->m_smokeTableID == ct_NULLID &&
           attribute->m_smokeAttrID.isNUL();
}

bool OrphanAttributeState_ResolveReferences(SimulationContext *context)
{
    AttributeOrphan *attribute = OrphanDefaultAttribute(context);
    int explosionTable = ct_NULLID;
    int explosionAttribute = ct_NULLID;
    ct_ClassTableID smokeTable = ct_NULLID;
    KR_ObjectID smokeAttribute = KR_ObjectID::NUL();
    if (!ResolveOrphanReferences(context, attribute, &explosionTable,
                                 &explosionAttribute, &smokeTable,
                                 &smokeAttribute))
        return false;
    attribute->m_cacheExplosionTable = explosionTable;
    attribute->m_cacheExplAttr = explosionAttribute;
    attribute->m_smokeTableID = smokeTable;
    attribute->m_smokeAttrID = smokeAttribute;
    return true;
}

bool OrphanAttributeState_ReferencesResolved(SimulationContext *context)
{
    AttributeOrphan *attribute = OrphanDefaultAttribute(context);
    int explosionTable = ct_NULLID;
    int explosionAttribute = ct_NULLID;
    ct_ClassTableID smokeTable = ct_NULLID;
    KR_ObjectID smokeAttribute = KR_ObjectID::NUL();
    return ResolveOrphanReferences(context, attribute, &explosionTable,
                                   &explosionAttribute, &smokeTable,
                                   &smokeAttribute) &&
           attribute->m_cacheExplosionTable == explosionTable &&
           attribute->m_cacheExplAttr == explosionAttribute &&
           attribute->m_smokeTableID == smokeTable &&
           attribute->m_smokeAttrID == smokeAttribute;
}

bool OrphanAttributeState_RuntimeReady(SimulationContext *context)
{
    return OrphanAttributeState_ReferencesResolved(context);
}

unsigned long long OrphanAttributeState_ReferenceFingerprint(
    SimulationContext *context)
{
    AttributeOrphan *attribute = OrphanDefaultAttribute(context);
    if (attribute == NULL ||
        !OrphanAttributeState_ReferencesResolved(context))
        return 0;
    unsigned long long hash = kOrphanReferenceHashOffset;
    OrphanReferenceHashString(hash, "Orphan.Attr.Default");
    OrphanReferenceHashString(hash, attribute->m_explAttrName);
    OrphanReferenceHashString(hash, attribute->m_smokeAttrName);
    const char *explosionTable = g_arena.searchSeanceClassTable(
        attribute->m_cacheExplosionTable);
    const char *smokeTable = g_arena.searchSeanceClassTable(
        attribute->m_smokeTableID);
    OrphanReferenceHashString(hash, explosionTable);
    OrphanReferenceHashString(hash, smokeTable);
    OrphanReferenceHashString(hash,
        context->searchObject(attribute->m_smokeAttrID));
    OrphanReferenceHashBytes(hash, &attribute->m_cacheExplAttr,
                             sizeof(attribute->m_cacheExplAttr));
    return hash;
}

void OrphanAttributeState_ClearReferences(SimulationContext *context)
{
    AttributeOrphan *attribute = OrphanDefaultAttribute(context);
    if (attribute == NULL)
        return;
    attribute->m_cacheExplosionTable = ct_NULLID;
    attribute->m_cacheExplAttr = ct_NULLID;
    attribute->m_smokeTableID = ct_NULLID;
    attribute->m_smokeAttrID = KR_ObjectID::NUL();
}
