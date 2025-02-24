#ifndef TACHYON_C_ZK_PLONK_HALO2_PROVER_IMPL_BASE_H_
#define TACHYON_C_ZK_PLONK_HALO2_PROVER_IMPL_BASE_H_

#include <stdint.h>

#include <memory>
#include <utility>

#include "tachyon/base/environment.h"
#include "tachyon/base/files/file_util.h"
#include "tachyon/base/functional/callback.h"
#include "tachyon/base/logging.h"
#include "tachyon/zk/plonk/halo2/prover.h"

namespace tachyon::c::zk::plonk::halo2 {

template <typename PCS>
class ProverImplBase : public tachyon::zk::plonk::halo2::Prover<PCS> {
 public:
  using Base = tachyon::zk::plonk::halo2::Prover<PCS>;
  using Poly = typename PCS::Poly;
  using Evals = typename PCS::Evals;
  using Commitment = typename PCS::Commitment;
  using Callback = base::OnceCallback<Base()>;

  ProverImplBase(Callback callback, uint8_t transcript_type)
      : Base(std::move(callback).Run()), transcript_type_(transcript_type) {
    std::string_view pcs_params_str;
    if (base::Environment::Get("TACHYON_PCS_PARAMS_LOG_PATH",
                               &pcs_params_str)) {
      VLOG(1) << "Save pcs params to: " << pcs_params_str;
      base::Uint8VectorBuffer buffer;
      CHECK(buffer.Grow(base::EstimateSize(this->pcs_)));
      CHECK(buffer.Write(this->pcs_));
      CHECK(base::WriteFile(base::FilePath(pcs_params_str),
                            absl::MakeConstSpan(buffer.owned_buffer())));
    }
  }

  uint8_t transcript_type() const { return transcript_type_; }

  void SetRngState(absl::Span<const uint8_t> state) {
    base::ReadOnlyBuffer buffer(state.data(), state.size());
    uint32_t x, y, z, w;
    CHECK(buffer.Read32LE(&x));
    CHECK(buffer.Read32LE(&y));
    CHECK(buffer.Read32LE(&z));
    CHECK(buffer.Read32LE(&w));
    CHECK(buffer.Done());
    Base::SetRng(std::make_unique<crypto::XORShiftRNG>(
        crypto::XORShiftRNG::FromState(x, y, z, w)));
  }

  void SetRng(std::unique_ptr<crypto::XORShiftRNG> rng) {
    Base::SetRng(std::move(rng));
  }

  void SetTranscript(
      absl::Span<const uint8_t> state,
      std::unique_ptr<crypto::TranscriptWriter<Commitment>> writer) {
    std::string_view state_str;
    if (base::Environment::Get("TACHYON_TRANSCRIPT_STATE_LOG_PATH",
                               &state_str)) {
      VLOG(1) << "Save transcript state to: " << state_str;
      CHECK(base::WriteFile(base::FilePath(state_str), state));
    }

    this->transcript_ = std::move(writer);
  }

  void CreateProof(
      tachyon::zk::plonk::ProvingKey<Poly, Evals, Commitment>& proving_key,
      tachyon::zk::plonk::halo2::ArgumentData<Poly, Evals>* argument_data) {
    std::string_view arg_data_str;
    if (base::Environment::Get("TACHYON_ARG_DATA_LOG_PATH", &arg_data_str)) {
      VLOG(1) << "Save argument data to: " << arg_data_str;
      base::Uint8VectorBuffer buffer;
      CHECK(buffer.Grow(base::EstimateSize(*argument_data)));
      CHECK(buffer.Write(*argument_data));
      CHECK(base::WriteLargeFile(base::FilePath(arg_data_str),
                                 absl::MakeConstSpan(buffer.owned_buffer())));
    }

    Base::CreateProof(proving_key, argument_data);
  }

 protected:
  uint8_t transcript_type_;
};

}  // namespace tachyon::c::zk::plonk::halo2

#endif  // TACHYON_C_ZK_PLONK_HALO2_PROVER_IMPL_BASE_H_
