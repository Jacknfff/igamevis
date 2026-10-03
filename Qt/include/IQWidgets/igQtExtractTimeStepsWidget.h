//
// igQtExtractTimeStepsWidget：「保留指定时间步」左侧工具面板。
//
// 两种选择方式：按索引（用「+」添加行、手动填写帧索引）/ 按区间 + 步长。
// 点「执行」运行 ExtractTimeStepsFilter，输出对象通过 ExtractTimeStepsApplied 交给主窗口。
//

#pragma once

#include <IQCore/igQtExportModule.h>
#include <TimeSeries/iGameExtractTimeStepsFilter.h>
#include <iGameDataObject.h>

#include <QWidget>

#include <vector>

class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTableWidgetItem;

class IG_QT_MODULE_EXPORT igQtExtractTimeStepsWidget : public QWidget {
    Q_OBJECT

public:
    explicit igQtExtractTimeStepsWidget(QWidget* parent = nullptr);
    ~igQtExtractTimeStepsWidget() override = default;

    /// 由主窗口调用：记录当前输入模型并刷新界面
    void SetOriginDataObject(iGame::DataObject::Pointer object);

signals:
    /// 执行成功：输出对象交由主窗口加入模型树
    void ExtractTimeStepsApplied(iGame::DataObject::Pointer output);

private slots:
    /// 「执行」按钮：收集参数 -> 跑 filter -> 回显结果并抛出信号
    void Apply();

    /// 「重置」按钮：清空索引列表、区间参数复位
    void Reset();

    /// 切换选择模式时显示对应的参数区域
    void UpdateModeUi();

private:
    void BuildUi();
    void RebuildTimeStepTable();
    void SetStatus(const QString& text, bool isError = false);
    /// 按当前模式刷新状态栏预览（区间模式：将保留几个）
    void UpdateStatusPreview();
    /// 往索引列表追加一行（-1 表示留空让用户填）
    void AppendIndexRow(int index = -1);
    /// 根据该行索引刷新"时间值"单元格
    void RefreshTimeValueCell(int row);
    /// 读取索引列表；单元格不是整数时返回 false（并标红该单元格）
    bool CollectIndices(std::vector<int>& indices);

    iGame::DataObject::Pointer m_InputObject;                  // 当前输入模型
    iGame::ExtractTimeStepsFilter::Pointer m_Filter;           // 每次都新建，避免残留上次状态

    QLabel* m_infoLabel{nullptr};        // 模型/时间步概况
    QComboBox* m_modeCombo{nullptr};     // 选择模式：按索引 / 按区间+步长
    QWidget* m_indexPanel{nullptr};      // 索引模式区域（索引列表 + 增删按钮），区间模式下整块隐藏
    QTableWidget* m_indexTable{nullptr}; // 索引列表（索引 / 时间值），行由「+」添加
    QPushButton* m_addIndexButton{nullptr};
    QPushButton* m_removeIndexButton{nullptr};
    QPushButton* m_clearIndicesButton{nullptr};
    QWidget* m_rangePanel{nullptr};      // 区间模式参数（起 / 止 / 步长）
    QSpinBox* m_beginSpin{nullptr};
    QSpinBox* m_endSpin{nullptr};
    QSpinBox* m_intervalSpin{nullptr};
    QLabel* m_statusLabel{nullptr};      // 结果 / 错误提示
    QPushButton* m_applyButton{nullptr};
    QPushButton* m_resetButton{nullptr};
};
