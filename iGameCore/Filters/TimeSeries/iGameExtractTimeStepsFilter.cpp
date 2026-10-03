#include "iGameExtractTimeStepsFilter.h"

#include "iGameAttributeSet.h"
#include "iGameDrawObject.h"
#include "iGamePointSet.h"
#include "iGameStringArray.h"
#include "iGameStructuredMesh.h"
#include "iGameSurfaceMesh.h"
#include "iGameType.h"
#include "iGameUnstructuredMesh.h"
#include "iGameVolumeMesh.h"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <vector>

IGAME_NAMESPACE_BEGIN

ExtractTimeStepsFilter::ExtractTimeStepsFilter() {
    SetNumberOfInputs(1);
    SetNumberOfOutputs(1); 
}

int ExtractTimeStepsFilter::GetAvailableTimeStepCount() {
    auto input = this->GetInput(0);
    if (input == nullptr) { return 0; }

    auto inputFrames = input->PeekTimeFrames();
    if (inputFrames == nullptr) { return 0; }

    return static_cast<int>(inputFrames->GetTimeNum());
}

std::vector<float> ExtractTimeStepsFilter::GetAvailableTimeValues() {
    std::vector<float> timeValues;

    auto input = this->GetInput(0);
    if (input == nullptr) { return timeValues; }
    auto inputFrames = input->PeekTimeFrames();
    if (inputFrames == nullptr) { return timeValues; }

    const int timeStepCount = static_cast<int>(inputFrames->GetTimeNum());
    timeValues.reserve(static_cast<std::size_t>(timeStepCount));
    for (int index = 0; index < timeStepCount; ++index) {
        timeValues.push_back(inputFrames->GetTargetTimeFrame(static_cast<unsigned int>(index)).GetTimeValue());
    }
    return timeValues;
}

std::vector<int> ExtractTimeStepsFilter::BuildKeptIndices(int timeStepCount) const {
    std::vector<int> keptIndices;
    if (timeStepCount <= 0) { return keptIndices; }

    if (m_SelectionMode == SELECT_TIME_RANGE) {
        // 闭区间 [begin, end]；步长 <= 0 视为 1，end < 0 表示取到最后一帧
        const int interval = (m_TimeStepInterval > 0) ? m_TimeStepInterval : 1;
        const int begin = std::max(m_RangeBegin, 0);
        const int end = (m_RangeEnd < 0) ? (timeStepCount - 1) : std::min(m_RangeEnd, timeStepCount - 1);
        for (int index = begin; index <= end; index += interval) {
            keptIndices.push_back(index);
        }
    } else {
        keptIndices = m_TimeStepIndices;
    }

    // 越界直接丢弃
    keptIndices.erase(
            std::remove_if(keptIndices.begin(), keptIndices.end(),
                           [timeStepCount](int index) { return index < 0 || index >= timeStepCount; }),
            keptIndices.end());

    // 去重 + 升序
    std::sort(keptIndices.begin(), keptIndices.end());
    keptIndices.erase(std::unique(keptIndices.begin(), keptIndices.end()), keptIndices.end());

    // 没有有效选择时保留全部时间步
    if (keptIndices.empty()) {
        igDebug("ExtractTimeStepsFilter: 未选中任何有效时间步，保留全部 {} 个时间步", timeStepCount);
        keptIndices.resize(static_cast<std::size_t>(timeStepCount));
        std::iota(keptIndices.begin(), keptIndices.end(), 0);
    }
    return keptIndices;
}

StreamingData::Pointer ExtractTimeStepsFilter::BuildOutputTimeFrames(const StreamingData::Pointer& inputFrames,
                                                                    const std::vector<int>& keptIndices) {
    if (inputFrames == nullptr) { return nullptr; }

    auto outputFrames = StreamingData::New();
    for (int index : keptIndices) {
        auto& inputFrame = inputFrames->GetTargetTimeFrame(static_cast<unsigned int>(index));

        // 时间值 / 元数据（文件路径列表）/ 帧类型原样搬运：时间值不重编号，帧数据仍然按需加载
        outputFrames->AddTimeStep(inputFrame.GetTimeValue(), inputFrame.GetMetaData(), inputFrame.GetFrameType());

        // 已经读进内存的帧：连缓存数据一起带过去，避免裁剪后重新读盘
        if (inputFrame.GetISCached()) {
            const auto newIndex = static_cast<unsigned int>(outputFrames->GetTimeNum() - 1);
            outputFrames->GetTargetTimeFrame(newIndex).SetCache(inputFrame.GetCachedData());
        }
    }

    // 保留输入的缓存策略：StreamingData 没有拷贝接口，用最大缓存帧数复现
    if (inputFrames->GetMaxCacheSize() > 0) {
        outputFrames->EnableCache(inputFrames->GetMaxCacheSize());
    }
    return outputFrames;
}

DataObject::Pointer ExtractTimeStepsFilter::CreateOutputLike(const DataObject::Pointer& input) {
    if (input == nullptr) { return nullptr; }

    switch (input->GetDataObjectType()) {
        case IG_SURFACE_MESH: {
            auto inputMesh = DynamicCast<SurfaceMesh>(input);
            auto outputMesh = SurfaceMesh::New();
            if (inputMesh.IsNotNull()) {
                outputMesh->ShallowCopy(inputMesh);
            }
            return outputMesh;
        }
        case IG_UNSTRUCTURED_MESH: {
            auto inputMesh = DynamicCast<UnstructuredMesh>(input);
            auto outputMesh = UnstructuredMesh::New();
            if (inputMesh.IsNotNull()) {
                outputMesh->SetPoints(inputMesh->GetPoints());
                outputMesh->SetCells(inputMesh->GetCells(), inputMesh->GetCellTypes());
                outputMesh->SetAttributeSet(inputMesh->GetAttributeSet());
            }
            return outputMesh;
        }
        case IG_VOLUME_MESH: {
            auto inputMesh = DynamicCast<VolumeMesh>(input);
            auto outputMesh = VolumeMesh::New();
            if (inputMesh.IsNotNull()) {
                outputMesh->SetVolumes(inputMesh->GetVolumes());
                outputMesh->SetPoints(inputMesh->GetPoints());
                outputMesh->SetAttributeSet(inputMesh->GetAttributeSet());
            }
            return outputMesh;
        }
        case IG_STRUCTURED_MESH: {
            auto inputMesh = DynamicCast<StructuredMesh>(input);
            auto outputMesh = StructuredMesh::New();
            if (inputMesh.IsNotNull()) {
                outputMesh->SetDimensionSize(inputMesh->GetDimensionSize());
                outputMesh->SetExtent(inputMesh->GetExtent());
                outputMesh->SetPoints(inputMesh->GetPoints());
                outputMesh->SetAttributeSet(inputMesh->GetAttributeSet());
            }
            return outputMesh;
        }
        case IG_POINT_SET: {
            auto inputMesh = DynamicCast<PointSet>(input);
            auto outputMesh = PointSet::New();
            if (inputMesh.IsNotNull()) {
                outputMesh->SetPoints(inputMesh->GetPoints());
                outputMesh->SetAttributeSet(inputMesh->GetAttributeSet());
            }
            return outputMesh;
        }
        case IG_DRAW_OBJECT: {
            auto inputObject = DynamicCast<DrawObject>(input);
            auto outputObject = DrawObject::New();
            if (inputObject.IsNull()) { return outputObject; }

            outputObject->SetAttributeSet(inputObject->GetAttributeSet());
            outputObject->SetColorMapper(inputObject->GetColorMapper());
            outputObject->SetViewStyle(static_cast<IGenum>(inputObject->GetViewStyle()));
            outputObject->SetPointSize(static_cast<float>(inputObject->GetPointSize()));
            outputObject->SetLineWidth(static_cast<float>(inputObject->GetLineWidth()));
            outputObject->SetTransparency(inputObject->GetTransparency());
            outputObject->SetVisibility(inputObject->GetVisibility());
            outputObject->SetAttributeIndex(inputObject->GetAttributeIndex());

            std::vector<DataObject::Pointer> children;
            children.reserve(static_cast<std::size_t>(inputObject->GetNumberOfSubDataObjects()));
            for (auto it = inputObject->SubDataObjectIteratorBegin(); it != inputObject->SubDataObjectIteratorEnd(); ++it) {
                children.push_back(it->second);
            }
            for (auto& child : children) {
                if (child == nullptr) { continue; }
                auto originalColorMapper = child->GetColorMapper();
                outputObject->AddSubDataObject(child);
                child->SetParentDataObject(inputObject.GetPointer());
                child->SetColorMapper(originalColorMapper);
            }
            return outputObject;
        }
        default:
            break;
    }

    // 其它类型：退化为基类对象
    auto output = DataObject::CreateDataObject(input->GetDataObjectType());
    if (output == nullptr) { output = DataObject::New(); }
    output->SetAttributeSet(input->GetAttributeSet());
    return output;
}

bool ExtractTimeStepsFilter::Execute() {
    this->UpdateProgress(0.0);

    auto input = this->GetInput(0);
    if (input == nullptr) {
        igDebug("ExtractTimeStepsFilter: 输入为空");
        return false;
    }

    // 输入必须是带时间序列的对象
    auto inputFrames = input->PeekTimeFrames();
    if (inputFrames == nullptr || inputFrames->GetTimeNum() == 0) {
        igDebug("ExtractTimeStepsFilter: 输入没有时间序列（TimeFrames 为空），无法裁剪时间步");
        return false;
    }

    const int timeStepCount = static_cast<int>(inputFrames->GetTimeNum());
    const std::vector<int> keptIndices = BuildKeptIndices(timeStepCount);

    auto outputFrames = BuildOutputTimeFrames(inputFrames, keptIndices);
    if (outputFrames == nullptr || outputFrames->GetTimeNum() == 0) {
        igDebug("ExtractTimeStepsFilter: 构建输出时间轴失败");
        return false;
    }

    auto output = CreateOutputLike(input);
    if (output == nullptr) {
        igDebug("ExtractTimeStepsFilter: 创建输出对象失败");
        return false;
    }
    output->SetName(input->GetName());
    output->SetTimeFrames(outputFrames); 

    // 记录本次结果
    m_KeptIndices = keptIndices;
    m_KeptTimeValues.clear();
    m_KeptTimeValues.reserve(keptIndices.size());
    for (int index : keptIndices) {
        m_KeptTimeValues.push_back(inputFrames->GetTargetTimeFrame(static_cast<unsigned int>(index)).GetTimeValue());
    }

    this->SetOutput(output);
    this->UpdateProgress(1.0);
    return true;
}

IGAME_NAMESPACE_END
