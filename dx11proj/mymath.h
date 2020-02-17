#pragma once


template<typename T>
size_t arr_size(T arr)
{
	return sizeof(arr) / sizeof(arr[0]);
}