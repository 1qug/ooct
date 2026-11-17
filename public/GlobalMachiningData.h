#ifndef GLOBALMACHININGDATA_H
#define GLOBALMACHININGDATA_H

#include <QString>
#include <QList>
#include <QMap>

class GlobalMachiningData
{
public:
    struct Coordinate {
        QString name;
        double originX, originY, originZ;          // 原点位置
        double xAxisX, xAxisY, xAxisZ;             // X轴方向向量
        double yAxisX, yAxisY, yAxisZ;             // Y轴方向向量
        double zAxisX, zAxisY, zAxisZ;             // Z轴方向向量

        Coordinate() : originX(0), originY(0), originZ(0),
            xAxisX(1), xAxisY(0), xAxisZ(0),
            yAxisX(0), yAxisY(1), yAxisZ(0),
            zAxisX(0), zAxisY(0), zAxisZ(1) {}
        Coordinate(const QString& n,
                   double ox, double oy, double oz,
                   double xx, double xy, double xz,
                   double yx, double yy, double yz,
                   double zx, double zy, double zz)
            : name(n), originX(ox), originY(oy), originZ(oz),
            xAxisX(xx), xAxisY(xy), xAxisZ(xz),
            yAxisX(yx), yAxisY(yy), yAxisZ(yz),
            zAxisX(zx), zAxisY(zy), zAxisZ(zz) {}
    };

    struct CuttingTool {
        QString toolNumber;
        double diameter;
        double lowerRadius;
        double coneAngle;
        double sharpCorner;
        double length;
        bool isShowhilt;
        double hiltUpperDiameter;
        double hiltLowerDiameter;
        double hiltLength;
        double hiltConeAngle;

        CuttingTool()
            : diameter(3), lowerRadius(0), coneAngle(0), sharpCorner(0), length(0),
            isShowhilt(false), hiltUpperDiameter(0), hiltLowerDiameter(0),
            hiltLength(0), hiltConeAngle(0) {}
        CuttingTool(const QString& num, double d, double lr, double ca, double sc, double len,
                    bool showHilt = false, double hUD = 0, double hLD = 0, double hL = 0, double hCA = 0)
            : toolNumber(num), diameter(d), lowerRadius(lr), coneAngle(ca), sharpCorner(sc), length(len),
            isShowhilt(showHilt), hiltUpperDiameter(hUD), hiltLowerDiameter(hLD),
            hiltLength(hL), hiltConeAngle(hCA) {}
    };

    struct Stock {
        QString name;
        int type;
        // 方体
        double length, width, height;
        // 圆柱
        double diameter, cylHeight;
        // 公共
        double centerX, centerY, centerZ;
        double angle;
        // 自动调整选项
        bool autoCenter;
        bool autoAngle;
        bool autoSize;

        Stock() : type(0), length(100), width(100), height(100),
            diameter(0), cylHeight(0), centerX(0), centerY(0), centerZ(0), angle(0),
            autoCenter(true), autoAngle(true), autoSize(true) {}
        Stock(const QString& n, int t = 0) : name(n), type(t), length(0), width(0), height(0),
            diameter(0), cylHeight(0), centerX(0), centerY(0), centerZ(0), angle(0),
            autoCenter(true), autoAngle(true), autoSize(true) {}
    };

    struct MachiningParams {
        QString paramName;
        QString workStation;
        QString currentCoordinateName;
        QString currentToolNumber;
        QString currentStockName;
        QString componentModelPath;
        QString top;
        QString bottom;
        double layerHeight;
        double stepover;
        int cuttingStyle;
        int cuttingDirection;
        int enterMode;
        double cuttingHeight;
        double spiralRadius;
        double spiralSlope;
        int feedMode;
        double arcRadius;
        double lineLength;
        bool returnCustom;
        int returnMode;
        int safePlaneType;
        double safeHeight;
        double sideMargin;
        double bottomMargin;
        double tolerance;
        double makeFeed;
        double cuttingFeed;
        double processingSpeed;

        MachiningParams()
            : layerHeight(2.0), stepover(1.0), cuttingStyle(0), cuttingDirection(0),
            enterMode(0), cuttingHeight(5.0), spiralRadius(80.0), spiralSlope(5.0),
            feedMode(0), arcRadius(10.0), lineLength(5.0), returnCustom(false), returnMode(0),
            safePlaneType(0), safeHeight(50.0), sideMargin(0.1), bottomMargin(0.1), tolerance(0.01),
            makeFeed(1000.0), cuttingFeed(1000.0), processingSpeed(2000.0) {}
    };

    static GlobalMachiningData& instance();

    QList<Coordinate> getCoordinates() const { return m_coordinates; }
    void addCoordinate(const Coordinate& coord);
    void updateCoordinate(int index, const Coordinate& coord);
    void deleteCoordinate(int index);
    Coordinate getCoordinateByName(const QString& name) const;
    int getCoordinateIndexByName(const QString& name) const;

    QList<CuttingTool> getTools() const { return m_tools; }
    void addTool(const CuttingTool& tool);
    void updateTool(int index, const CuttingTool& tool);
    void deleteTool(int index);
    CuttingTool getToolByNumber(const QString& number) const;
    int getToolIndexByNumber(const QString& number) const;

    QList<Stock> getStocks() const { return m_stocks; }
    void addStock(const Stock& stock);
    void updateStock(int index, const Stock& stock);
    void deleteStock(int index);
    Stock getStockByName(const QString& name) const;
    int getStockIndexByName(const QString& name) const;
    QString generateStockName(const Stock& stock) const;

    void saveCurrentParams(const QString& name, const MachiningParams& params);
    MachiningParams loadCurrentParams(const QString& name) const;
    QStringList getSavedParamNames() const { return m_savedParamsMap.keys(); }
    void deleteSavedParam(const QString& name);
    MachiningParams getCurrentParams() const { return m_currentParams; }
    bool updateMachiningParams(const QString &oldName, const MachiningParams &newParams);
    bool getMachiningParams(const QString& name, MachiningParams& outParams) const;

    QString currentCoordinateName;
    QString currentToolNumber;
    QString currentStockName;
    QString componentModelPath;
    int currentStockIndex = -1;

private:
    GlobalMachiningData() {}
    QList<Coordinate> m_coordinates;
    QList<CuttingTool> m_tools;
    QList<Stock> m_stocks;
    QMap<QString, MachiningParams> m_savedParamsMap;
    MachiningParams m_currentParams;
};

#endif // GLOBALMACHININGDATA_H
