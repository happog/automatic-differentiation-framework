#include <iostream>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include "../include/Tensor.h"
#include "../include/multi_thread/matrix_task.h"
#include "../include/multi_thread/thread_pool.h"
using namespace std;

Tensor::Tensor (vector<int> shape, int need_init) {
    m_size = 1;
    for (int i = 0; i < shape.size (); ++i) {
        m_shape.push_back (shape[i]);
        m_size *= shape[i];
    }
    m_tensor = new float[m_size];
    if (need_init == 1) {
        for (int i = 0; i < m_size; ++i) {
            m_tensor[i] = 0.0;
        }
    }
}

Tensor::Tensor (vector<int> shape, float data[]) {
    m_size = 1;
    for (int i = 0; i < shape.size (); ++i) {
        m_shape.push_back (shape[i]);
        m_size *= shape[i];
    }
    m_tensor = new float[m_size];
    for (int i = 0; i < m_size; ++i) {
        m_tensor[i] = data[i];
    }
}

float Tensor::get_value (vector<int> idxs) {
    int idx = 0;
    int t = 1;
    for (int i = idxs.size () - 1; i >= 0; --i) {
        idx += idxs[i] * t;
        t *= m_shape[i];
    }
    return m_tensor[idx];
}

void Tensor::set_value (vector<int> idxs, float value) {
    int idx = 0;
    int t = 1;
    for (int i = idxs.size () - 1; i >= 0; --i) {
        idx += idxs[i] * t;
        t *= m_shape[i];
    }
    m_tensor[idx] = value;
}

Tensor* Tensor::matrix_mult (Tensor* tensor) {
    Tensor* result = 0;
    if (m_shape[1] == tensor -> m_shape[0]) {
        vector<int> result_shape (2);
        result_shape[0] = m_shape[0];
        result_shape[1] = tensor -> m_shape[1];
        result = new Tensor (result_shape, 0);
        int idx0 = 0, idx1 = 0, idx2 = 0;
        vector<task*> task_list;
        for (int i = 0; i < m_shape[0]; ++i) {
            for (int j = 0; j < tensor -> m_shape[1]; ++j) {
                /*float r = 0;
                float compensation = 0.0;
                for (int k = 0; k < m_shape[1]; ++k) {
                    idx0 = i * m_shape[1] + k;
                    idx1 = k * tensor -> m_shape[1] + j;
                    // Kahan's Summation Formula
                    // r += m_tensor[idx0] * tensor -> m_tensor[idx1];
                    float y = m_tensor[idx0] * tensor -> m_tensor[idx1] - compensation;// 补偿
                    float t = r + y;// 发生舍入
                    compensation = (t - r) - y;// 记录下舍入误差
                    r = t;
                }
                idx2 = i * tensor -> m_shape[1] + j;
                result -> m_tensor[idx2] = r;*/
                task_list.push_back (new matrix_mult_task (this, tensor, result, i, j));
            }
        }
        (thread_pool::get_instance ()) -> add_job_list (task_list);
    }
    return result;
}

Tensor* Tensor::scalar_mult (float scalar) {
    Tensor* result = new Tensor (m_shape, 0);
    /*for (int i = 0; i < m_size; ++i) {
        result -> m_tensor[i] = m_tensor[i] * scalar;
    }*/
    vector<task*> task_list;
    int thread_num = (thread_pool::get_instance ()) -> m_worker_num;
    for (int i = 0; i < thread_num; ++i) {
        task_list.push_back (new matrix_scalar_mult_task (this, scalar, result, i, thread_num));
    }
    (thread_pool::get_instance ()) -> add_job_list (task_list);
    return result;
}

void Tensor::scalar_acc_mult (float scalar) {
    /*for (int i = 0; i < m_size; ++i) {
        m_tensor[i] = m_tensor[i] * scalar;
    }*/
    vector<task*> task_list;
    int thread_num = (thread_pool::get_instance ()) -> m_worker_num;
    for (int i = 0; i < thread_num; ++i) {
        task_list.push_back (new matrix_scalar_mult_task (this, scalar, this, i, thread_num));
    }
    (thread_pool::get_instance ()) -> add_job_list (task_list);
}

void Tensor::element_square () {
    for (int i = 0; i < m_size; ++i) {
        m_tensor[i] = m_tensor[i] * m_tensor[i];
    }
}

float Tensor::element_abs_sum () {
    float result = 0;
    float compensation = 0.0;
    for (int i = 0; i < m_size; ++i) {
        // result += fabs (m_tensor[i]);
        // Kahan's Summation Formula
        float y = fabs (m_tensor[i]) - compensation;// 补偿
        float t = result + y;// 发生舍入
        compensation = (t - result) - y;// 记录本次的舍入误差
        result = t;
    }
    return result;
}

float Tensor::element_square_sum () {
    float result = 0;
    float compensation = 0.0;
    for (int i = 0; i < m_size; ++i) {
        // result += m_tensor[i] * m_tensor[i];
        // Kahan's Summation Formula
        float y = m_tensor[i] * m_tensor[i] - compensation;
        float t = result + y;
        compensation = (t - result) - y;
        result = t;
    }
    return result;
}

Tensor* Tensor::element_mult (Tensor* tensor) {
    Tensor* result = 0;
    int same_shape = 1;
    if (m_shape.size () == tensor -> m_shape.size ()) {
        for (int i = 0; i < m_shape.size (); ++i) {
            if (m_shape[i] != tensor -> m_shape[i]) {
                same_shape = 0;
                break;
            }
        }
    } else {
        same_shape = 0;
    }
    if (same_shape == 1) {
        result = new Tensor (tensor -> m_shape, 0);
        for (int i = 0; i < m_size; ++i) {
            result -> m_tensor[i] = m_tensor[i] * tensor -> m_tensor[i];
        }
    }
    return result;
}

void Tensor::add (Tensor* tensor, Tensor* result) {
    /*for (int i = 0; i < m_size; ++i) {
        result -> m_tensor[i] = m_tensor[i] + tensor -> m_tensor[i];
    }*/
    vector<task*> task_list;
    int thread_num = (thread_pool::get_instance ()) -> m_worker_num;
    for (int i = 0; i < thread_num; ++i) {
        task_list.push_back (new matrix_add_task (this, tensor, result, i, thread_num));
    }
    (thread_pool::get_instance ()) -> add_job_list (task_list);
}

Tensor* Tensor::add (Tensor* tensor) {
    Tensor* result = 0;
    int same_shape = 1;
    if (m_shape.size () == tensor -> m_shape.size ()) {
        for (int i = 0; i < m_shape.size (); ++i) {
            if (m_shape[i] != tensor -> m_shape[i]) {
                same_shape = 0;
                break;
            }
        }
    } else {
        same_shape = 0;
    }

    if (same_shape == 1) {
        result = new Tensor (tensor -> m_shape, 0);
        /*for (int i = 0; i < m_size; ++i) {
            result -> m_tensor[i] = m_tensor[i] + tensor -> m_tensor[i];
        }*/
        vector<task*> task_list;
        int thread_num = (thread_pool::get_instance ()) -> m_worker_num;
        for (int i = 0; i < thread_num; ++i) {
            task_list.push_back (new matrix_add_task (this, tensor, result, i, thread_num));
        }
        (thread_pool::get_instance ()) -> add_job_list (task_list);
    }
    return result;
}

void Tensor::init () {
    // srand (time (0));
    for (int i = 0; i < m_size; ++i) {
        m_tensor[i] = (rand () % 1000) / 1000.0 - 0.5;
    }
}

void Tensor::display () {
    vector<int> idxs0 (2);
    for (int i = 0; i < m_shape[0]; ++i) {
        for (int j = 0; j < m_shape[1]; ++j) {
            idxs0[0] = i; idxs0[1] = j;
            cout << get_value (idxs0) << " ";
        }
        cout << endl;
    }
}

Tensor::~Tensor () {
    delete[] m_tensor;
}
