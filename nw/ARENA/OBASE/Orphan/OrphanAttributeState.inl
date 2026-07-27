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
