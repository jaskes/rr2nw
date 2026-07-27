AttributeTableBird __attrBirdTable;

void BirdAttributeState_Link()
{
}

void AttributeTableBird::allocObjects(int objectQnty)
{
    m_table = new AttributeBird[objectQnty];
    if (m_table == NULL)
        m_maxObjectQnty = 0;
}

void AttributeTableBird::freeObjects()
{
    delete [] m_table;
    m_table = NULL;
    m_maxObjectQnty = 0;
}

ct_Object *AttributeTableBird::getObjectPTR(int index)
{
    s_ASSERT(index >= 0 && index < m_maxObjectQnty,
             "AttributeTableBird::getObjectPTR");
    return &(m_table[index]);
}

void AttributeBird::update(double ts)
{
    KR_ObjectID skinID = context->searchObject(m_skinName);
    s_ASSERT(!skinID.isNUL(), "AttributeBird::update");

    KR_Event event;
    event.timeStamp = ts;
    event.label = sk_EV_QUERY_MODEL_PTR;
    event.destination = skinID;
    context->sendEventNow(event);
    s_ASSERT(event.label == sk_EV_QUERY_MODEL_PTR_OK,
             "AttributeBird::update");
    event.data.open(EDO_READ)
              .get(&m_cacheSkin, sizeof(void *))
              .close();
}

bool BirdAttributeState_IsRetailDefault(const KR_ObjectID &objectID)
{
    AttributeBird *attr = static_cast<AttributeBird *>(
        __attrBirdTable.searchAttribute(objectID));
    if (attr == NULL)
        return false;

    const double speedDifference =
        attr->m_speed > 2.0 ? attr->m_speed - 2.0 : 2.0 - attr->m_speed;
    const double positionDifference =
        attr->m_calcPosIncrement > 0.2
            ? attr->m_calcPosIncrement - 0.2
            : 0.2 - attr->m_calcPosIncrement;
    return strcmp(attr->m_skinName, "sk.Bird.0") == 0 &&
           attr->m_calcNewPosIncrement == 5.0 &&
           speedDifference <= 1.0e-6 &&
           positionDifference <= 1.0e-6 && attr->m_cacheSkin == NULL;
}
