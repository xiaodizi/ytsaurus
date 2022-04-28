#include "context.h"

#include <yt/yt/server/master/cell_master/bootstrap.h>

#include <yt/yt/server/lib/hive/hive_manager.h>

#include <yt/yt/server/lib/tablet_node/proto/tablet_manager.pb.h>

#include <yt/yt/ytlib/sequoia_client/tables.h>

#include <yt/yt/ytlib/api/native/tablet_request_batcher.h>
#include <yt/yt/ytlib/api/native/transaction_helpers.h>

#include <yt/yt/client/table_client/row_buffer.h>

namespace NYT::NSequoiaServer {

using namespace NApi::NNative;
using namespace NCellMaster;
using namespace NSequoiaClient;
using namespace NTableClient;
using namespace NTabletClient;
using namespace NTransactionClient;

////////////////////////////////////////////////////////////////////////////////

class TSequoiaContext
    : public ISequoiaContext
{
public:
    TSequoiaContext(
        TBootstrap* bootstrap,
        TTransactionId transactionId,
        const NSequoiaClient::NProto::TWriteSet& protoWriteSet)
        : Bootstrap_(bootstrap)
        , TransactionId_(transactionId)
    {
        FromProto(&WriteSet_, protoWriteSet, RowBuffer_);

        for (auto table : TEnumTraits<ESequoiaTable>::GetDomainValues()) {
            auto tableDescriptor = GetTableDescriptor(table);
            for (const auto& [key, lockedRowInfo] : WriteSet_[table]) {
                auto tabletId = lockedRowInfo.TabletId;
                auto cellId = lockedRowInfo.TabletCellId;

                if (!CellIdToSignatureGenerator_.contains(cellId)) {
                    EmplaceOrCrash(
                        CellIdToSignatureGenerator_,
                        cellId,
                        std::make_unique<TTransactionSignatureGenerator>(/*tagetSignature*/ 1u));
                }
                if (!TabletIdToRequestBatcher_.contains(tabletId)) {
                    EmplaceOrCrash(
                        TabletIdToRequestBatcher_,
                        tabletId,
                        CreateTabletRequestBatcher(
                            TTabletRequestBatcherOptions{}, // TODO
                            tableDescriptor->GetSchema(),
                            tableDescriptor->GetColumnEvaluator()));
                    EmplaceOrCrash(TabletIdToTabletCellId_, tabletId, cellId);
                }
            }
        }
    }

    void WriteRow(ESequoiaTable table, TUnversionedRow row) override
    {
        auto tableDescriptor = GetTableDescriptor(table);
        auto keyColumnCount = tableDescriptor->GetSchema()->GetKeyColumnCount();
        auto key = GetKeyPrefix(row, keyColumnCount, RowBuffer_);
        const auto& lockedRowInfo = GetOrCrash(WriteSet_[table], key);
        const auto& tabletRequestBatcher = GetOrCrash(TabletIdToRequestBatcher_, lockedRowInfo.TabletId);
        tabletRequestBatcher->SubmitUnversionedRow(EWireProtocolCommand::WriteAndLockRow, row, TLockMask{});
    }

    void DeleteRow(ESequoiaTable table, TLegacyKey key) override
    {
        const auto& lockedRowInfo = GetOrCrash(WriteSet_[table], key);
        const auto& tabletRequestBatcher = GetOrCrash(TabletIdToRequestBatcher_, lockedRowInfo.TabletId);
        tabletRequestBatcher->SubmitUnversionedRow(EWireProtocolCommand::DeleteRow, key, TLockMask{});
    }

    void SubmitRows() override
    {
        THashMap<TTabletId, std::vector<std::unique_ptr<ITabletRequestBatcher::TBatch>>> tabletIdToBatches;

        for (const auto& [tabletId, tabletRequestBatcher] : TabletIdToRequestBatcher_) {
            auto batches = tabletRequestBatcher->PrepareBatches();
            auto requestCount = std::max<int>(std::ssize(batches), 1);

            auto* cellSignatureGenerator = GetOrCrash(
                CellIdToSignatureGenerator_,
                GetOrCrash(TabletIdToTabletCellId_, tabletId)).get();
            cellSignatureGenerator->RegisterRequests(requestCount);

            EmplaceOrCrash(tabletIdToBatches, tabletId, std::move(batches));
        }

        for (const auto& [tabletId, batches] : tabletIdToBatches) {
            for (auto& batch : batches) {
                YT_VERIFY(batch->RowCount > 0);

                // TODO(gritukan): Delayed write.
                Y_UNUSED(Bootstrap_);
            }
        }
    }

    const TRowBufferPtr& GetRowBuffer() const override
    {
        return RowBuffer_;
    }

private:
    TBootstrap* Bootstrap_;
    const TTransactionId TransactionId_;
    TWriteSet WriteSet_;

    THashMap<TTabletCellId, std::unique_ptr<TTransactionSignatureGenerator>> CellIdToSignatureGenerator_;
    THashMap<TTabletId, ITabletRequestBatcherPtr> TabletIdToRequestBatcher_;
    THashMap<TTabletId, TTabletCellId> TabletIdToTabletCellId_;

    struct TSequoiaContextTag
    { };
    const TRowBufferPtr RowBuffer_ = New<TRowBuffer>(TSequoiaContextTag());
};

////////////////////////////////////////////////////////////////////////////////

ISequoiaContextPtr CreateSequoiaContext(
    TBootstrap* bootstrap,
    TTransactionId transactionId,
    const NSequoiaClient::NProto::TWriteSet& protoWriteSet)
{
    return New<TSequoiaContext>(bootstrap, transactionId, protoWriteSet);
}

////////////////////////////////////////////////////////////////////////////////

thread_local ISequoiaContextPtr SequoiaContext;

void SetSequoiaContext(ISequoiaContextPtr context)
{
    SequoiaContext = std::move(context);
}

const ISequoiaContextPtr& GetSequoiaContext()
{
    return SequoiaContext;
}

////////////////////////////////////////////////////////////////////////////////

TSequoiaContextGuard::TSequoiaContextGuard(ISequoiaContextPtr context)
{
    SetSequoiaContext(std::move(context));
}

TSequoiaContextGuard::~TSequoiaContextGuard()
{
    if (SequoiaContext) {
        SequoiaContext->SubmitRows();
    }

    SetSequoiaContext(/*context*/ nullptr);
}

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NSequoiaServer