#include "tachyon/crypto/commitments/kzg/kzg.h"

#include "gtest/gtest.h"

#include "tachyon/base/buffer/buffer.h"
#include "tachyon/math/elliptic_curves/bn/bn254/g1.h"
#include "tachyon/math/polynomials/univariate/univariate_evaluation_domain_factory.h"

namespace tachyon::crypto {

namespace {

constexpr size_t K = 3;
constexpr size_t N = size_t{1} << K;
constexpr size_t kMaxDegree = N - 1;

class KZGTest : public testing::Test {
 public:
  using PCS =
      KZG<math::bn254::G1AffinePoint, kMaxDegree, math::bn254::G1AffinePoint>;
  using Domain = math::UnivariateEvaluationDomain<math::bn254::Fr, kMaxDegree>;
  using Poly = math::UnivariateDensePolynomial<math::bn254::Fr, kMaxDegree>;
  using Evals = math::UnivariateEvaluations<math::bn254::Fr, kMaxDegree>;

  static void SetUpTestSuite() { math::bn254::G1Curve::Init(); }
};

}  // namespace

TEST_F(KZGTest, UnsafeSetup) {
  PCS pcs;
  ASSERT_TRUE(pcs.UnsafeSetup(N));

  EXPECT_EQ(pcs.N(), N);
  EXPECT_EQ(pcs.g1_powers_of_tau().size(), size_t{N});
  EXPECT_EQ(pcs.g1_powers_of_tau_lagrange().size(), size_t{N});
}

TEST_F(KZGTest, CommitLagrange) {
  PCS pcs;
  ASSERT_TRUE(pcs.UnsafeSetup(N));

  Poly poly = Poly::Random(N - 1);

  math::bn254::G1AffinePoint commit;
  ASSERT_TRUE(pcs.Commit(poly.coefficients().coefficients(), &commit));

  std::unique_ptr<Domain> domain = Domain::Create(N);
  Evals poly_evals = domain->FFT(std::move(poly));

  math::bn254::G1AffinePoint commit_lagrange;
  ASSERT_TRUE(pcs.CommitLagrange(poly_evals.evaluations(), &commit_lagrange));

  EXPECT_EQ(commit, commit_lagrange);
}

TEST_F(KZGTest, BatchCommitLagrange) {
  PCS pcs;
  ASSERT_TRUE(pcs.UnsafeSetup(N));

  size_t num_polys = 10;
  std::vector<Poly> polys =
      base::CreateVector(num_polys, []() { return Poly::Random(N - 1); });

  BatchCommitmentState state(true, num_polys);
  pcs.ResizeBatchCommitments(num_polys);
  for (size_t i = 0; i < num_polys; ++i) {
    ASSERT_TRUE(pcs.Commit(polys[i].coefficients().coefficients(), state, i));
  }
  std::vector<math::bn254::G1AffinePoint> batch_commitments =
      pcs.GetBatchCommitments(state);
  EXPECT_EQ(state.batch_mode, false);
  EXPECT_EQ(state.batch_count, size_t{0});

  std::unique_ptr<Domain> domain = Domain::Create(N);
  std::vector<Evals> poly_evals = base::Map(
      polys, [&domain](Poly& poly) { return domain->FFT(std::move(poly)); });

  state.batch_mode = true;
  state.batch_count = num_polys;
  pcs.ResizeBatchCommitments(num_polys);
  for (size_t i = 0; i < num_polys; ++i) {
    ASSERT_TRUE(pcs.CommitLagrange(poly_evals[i].evaluations(), state, i));
  }
  std::vector<math::bn254::G1AffinePoint> batch_commitments_lagrange =
      pcs.GetBatchCommitments(state);
  EXPECT_EQ(state.batch_mode, false);
  EXPECT_EQ(state.batch_count, size_t{0});

  EXPECT_EQ(batch_commitments, batch_commitments_lagrange);
}

TEST_F(KZGTest, Downsize) {
  PCS pcs;
  ASSERT_TRUE(pcs.UnsafeSetup(N));
  ASSERT_FALSE(pcs.Downsize(N));
  ASSERT_TRUE(pcs.Downsize(N / 2));
  EXPECT_EQ(pcs.N(), N / 2);
}

TEST_F(KZGTest, Copyable) {
  PCS expected;
  ASSERT_TRUE(expected.UnsafeSetup(N));

  std::vector<uint8_t> vec;
  vec.resize(base::EstimateSize(expected));
  base::Buffer write_buf(vec.data(), vec.size());
  ASSERT_TRUE(write_buf.Write(expected));
  ASSERT_TRUE(write_buf.Done());

  write_buf.set_buffer_offset(0);

  PCS value;
  ASSERT_TRUE(write_buf.Read(&value));

  EXPECT_EQ(expected.g1_powers_of_tau(), value.g1_powers_of_tau());
  EXPECT_EQ(expected.g1_powers_of_tau_lagrange(),
            value.g1_powers_of_tau_lagrange());
}

}  // namespace tachyon::crypto
