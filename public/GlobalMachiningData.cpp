#include "GlobalMachiningData.h"

GlobalMachiningData& GlobalMachiningData::instance()
{
    static GlobalMachiningData instance;
    return instance;
}

// 坐标系
void GlobalMachiningData::addCoordinate(const Coordinate& coord)
{
    m_coordinates.append(coord);
}

void GlobalMachiningData::updateCoordinate(int index, const Coordinate& coord)
{
    if (index >= 0 && index < m_coordinates.size())
        m_coordinates[index] = coord;
}

void GlobalMachiningData::deleteCoordinate(int index)
{
    if (index >= 0 && index < m_coordinates.size())
        m_coordinates.removeAt(index);
}

GlobalMachiningData::Coordinate GlobalMachiningData::getCoordinateByName(const QString& name) const
{
    for (const auto& c : m_coordinates)
        if (c.name == name) return c;
    return Coordinate();
}

int GlobalMachiningData::getCoordinateIndexByName(const QString& name) const
{
    for (int i = 0; i < m_coordinates.size(); ++i)
        if (m_coordinates[i].name == name) return i;
    return -1;
}

// 刀具
void GlobalMachiningData::addTool(const CuttingTool& tool)
{
    m_tools.append(tool);
}

void GlobalMachiningData::updateTool(int index, const CuttingTool& tool)
{
    if (index >= 0 && index < m_tools.size())
        m_tools[index] = tool;
}

void GlobalMachiningData::deleteTool(int index)
{
    if (index >= 0 && index < m_tools.size())
        m_tools.removeAt(index);
}

GlobalMachiningData::CuttingTool GlobalMachiningData::getToolByNumber(const QString& number) const
{
    for (const auto& t : m_tools)
        if (t.toolNumber == number) return t;
    return CuttingTool();
}

int GlobalMachiningData::getToolIndexByNumber(const QString& number) const
{
    for (int i = 0; i < m_tools.size(); ++i)
        if (m_tools[i].toolNumber == number) return i;
    return -1;
}

// 毛坯
void GlobalMachiningData::addStock(const Stock& stock)
{
    m_stocks.append(stock);
}

void GlobalMachiningData::updateStock(int index, const Stock& stock)
{
    if (index >= 0 && index < m_stocks.size())
        m_stocks[index] = stock;
}

void GlobalMachiningData::deleteStock(int index)
{
    if (index >= 0 && index < m_stocks.size())
        m_stocks.removeAt(index);
}

GlobalMachiningData::Stock GlobalMachiningData::getStockByName(const QString& name) const
{
    for (const auto& s : m_stocks)
        if (generateStockName(s) == name) return s;
    return Stock();
}

int GlobalMachiningData::getStockIndexByName(const QString& name) const
{
    for (int i = 0; i < m_stocks.size(); ++i)
        if (generateStockName(m_stocks[i]) == name) return i;
    return -1;
}

QString GlobalMachiningData::generateStockName(const Stock& stock) const
{
    if (stock.type == 0)
        return QString("方体")/*.arg(stock.length).arg(stock.width).arg(stock.height)*/;
    else
        return QString("圆柱")/*.arg(stock.diameter).arg(stock.cylHeight)*/;
}

// 参数快照
void GlobalMachiningData::saveCurrentParams(const QString& name, const MachiningParams& params)
{
    if (!name.isEmpty()) {
        m_savedParamsMap[name] = params;
        m_currentParams = params;
    }
}

GlobalMachiningData::MachiningParams GlobalMachiningData::loadCurrentParams(const QString& name) const
{
    return m_savedParamsMap.value(name, MachiningParams());
}

void GlobalMachiningData::deleteSavedParam(const QString& name)
{
    m_savedParamsMap.remove(name);
}
// 在 GlobalMachiningData 类中添加
bool GlobalMachiningData::updateMachiningParams(const QString &oldName, const MachiningParams &newParams)
{
    auto it = m_savedParamsMap.find(oldName);
    if (it == m_savedParamsMap.end()) {
        return false;
    }
    // 如果名称改变，需要重新插入并删除旧键
    if (oldName != newParams.paramName) {
        m_savedParamsMap.erase(it);
        m_savedParamsMap[newParams.paramName] = newParams;
    } else {
        it.value() = newParams;   // 正确修改 QMap 中的值
    }
    return true;
}
bool GlobalMachiningData::getMachiningParams(const QString& name, MachiningParams& outParams) const
{
    auto it = m_savedParamsMap.constFind(name);
    if (it != m_savedParamsMap.constEnd()) {
        outParams = it.value();
        return true;
    }
    return false;
}
