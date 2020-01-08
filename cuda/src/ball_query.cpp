#include "ball_query.h"
#include "utils.h"

void query_ball_point_kernel_dense_wrapper(int b, int n, int m, float radius,
					   int nsample, const float *new_xyz,
					   const float *xyz, int *idx);

void query_ball_point_kernel_partial_wrapper(long batch_size,
					     int size_x,
					     int size_y, 
						 float radius, 
						 int nsample,
					     const float *x,
					     const float *y,
					     const long *batch_x,
					     const long *batch_y,
					     long *idx_out,
					     float *dist_out);

at::Tensor ball_query_dense(at::Tensor new_xyz, at::Tensor xyz, const float radius,
			    const int nsample) {
  CHECK_CONTIGUOUS(new_xyz);
  CHECK_CONTIGUOUS(xyz);
  CHECK_IS_FLOAT(new_xyz);
  CHECK_IS_FLOAT(xyz);

  if (new_xyz.type().is_cuda()) {
    CHECK_CUDA(xyz);
  }

  at::Tensor idx =
      torch::zeros({new_xyz.size(0), new_xyz.size(1), nsample},
                   at::device(new_xyz.device()).dtype(at::ScalarType::Int));

  if (new_xyz.type().is_cuda()) {
    query_ball_point_kernel_dense_wrapper(xyz.size(0), xyz.size(1), new_xyz.size(1),
					  radius, nsample, new_xyz.data<float>(),
					  xyz.data<float>(), idx.data<int>());
  } else {
    AT_CHECK(false, "CPU not supported");
  }

  return idx;
}

at::Tensor degree(at::Tensor row, int64_t num_nodes) {
	auto zero = at::zeros(num_nodes, row.options());
	auto one = at::ones(row.size(0), row.options());
	return zero.scatter_add_(0, row, one);
  }

std::pair<at::Tensor, at::Tensor> ball_query_partial_dense(at::Tensor x,
							   at::Tensor y,
							   at::Tensor batch_x,
							   at::Tensor batch_y,
							   const float radius,
							   const int nsample) {
	CHECK_CONTIGUOUS(x);
	CHECK_CONTIGUOUS(y);
	CHECK_IS_FLOAT(x);
	CHECK_IS_FLOAT(y);

	if (x.type().is_cuda()) {
		CHECK_CUDA(x);
		CHECK_CUDA(y);
		CHECK_CUDA(batch_x);
		CHECK_CUDA(batch_y);
	}

	at::Tensor idx = torch::zeros({y.size(0), nsample},
				      at::device(y.device()).dtype(at::ScalarType::Long));
	at::Tensor dist = torch::zeros({y.size(0), nsample},
				      at::device(y.device()).dtype(at::ScalarType::Float));

	cudaSetDevice(x.get_device());
	auto batch_sizes = (int64_t *)malloc(sizeof(int64_t));
	cudaMemcpy(batch_sizes, batch_x[-1].data<int64_t>(), sizeof(int64_t),
				cudaMemcpyDeviceToHost);
	auto batch_size = batch_sizes[0] + 1;

	batch_x = degree(batch_x, batch_size);
	batch_x = at::cat({at::zeros(1, batch_x.options()), batch_x.cumsum(0)}, 0);
	batch_y = degree(batch_y, batch_size);
	batch_y = at::cat({at::zeros(1, batch_y.options()), batch_y.cumsum(0)}, 0);


	if (x.type().is_cuda()) {
		query_ball_point_kernel_partial_wrapper(batch_size,
							x.size(0),
							y.size(0),
							radius, nsample,
							x.data<float>(),
							y.data<float>(),
							batch_x.data<long>(),
							batch_y.data<long>(),
							idx.data<long>(),
							dist.data<float>());
	} else {
	  AT_CHECK(false, "CPU not supported");
	}

	return std::make_pair(idx, dist);
}
