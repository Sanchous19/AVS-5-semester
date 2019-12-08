#include <iostream>
#include <ctime>
#include <stdlib.h>
#include <clocale>
#include <omp.h>
#include <iomanip>

using namespace std;


__forceinline
void generate_array(int*** arr, const int& n)
{
	*arr = new int *[n];
	for (int i = 0; i < n; i++)
		(*arr)[i] = new int[n];
}


__forceinline
void delete_array(int*** arr, const int& n)
{
	for (int i = 0; i < n; i++)
		delete[] (*arr)[i];
	delete[] (*arr);
}


__forceinline
void fill_array(int*** arr, const int& n)
{
	for (int i = 0; i < n; i++)
		for (int j = 0; j < n; j++)
			(*arr)[i][j] = rand() % 10;
}


__forceinline
void fill_array0(int*** arr, const int& n)
{
	for (int i = 0; i < n; i++)
		for (int j = 0; j < n; j++)
			(*arr)[i][j] = 0;
}


int main()
{
	setlocale(LC_ALL, "russian");

	int n;
	cout << "Введите размерность матриц А и В: ";
	cin >> n;

	int **a, **b, **c1, **c2;

	generate_array(&a, n);
	generate_array(&b, n);
	generate_array(&c1, n);
	generate_array(&c2, n);

	fill_array(&a, n);
	fill_array(&b, n);
	fill_array0(&c1, n);

	double t1, t2, dt1, dt2;

	t1 = omp_get_wtime();
	for (int i = 0; i < n; i++)
		for (int j = 0; j < n; j++)
			for (int k = 0; k < n; k++)
				c1[i][j] += a[i][k] * b[k][j];
	t2 = omp_get_wtime();
	dt1 = t2 - t1;
	cout << "Время выполнения без параллельного вычисления = " << dt1 << " секунд " << endl;
	
	fill_array0(&c2, n);

	int i, j, k;
	omp_set_num_threads(omp_get_num_procs());
	t1 = omp_get_wtime();

	#pragma omp parallel for shared(a, b, c) private(i, j, k) schedule(static, n / omp_get_num_threads())
	for (i = 0; i < n; i++)
		for (j = 0; j < n; j++)
			for (k = 0; k < n; k++)
				c2[i][j] += a[i][k] * b[k][j];
	t2 = omp_get_wtime();
	dt2 = t2 - t1;
	cout << "Время выполнения с использованием параллельного вычисления= " << dt2 << " секунд " << endl;

	cout << "Выигрыш во времени = " << dt1 / dt2 << endl;

	
	bool is_simple = true;
	for (int i = 0; i < n; i++)
		for (int j = 0; j < n; j++)
			if (c1[i][j] != c2[i][j])
				is_simple = false;
	cout << is_simple;


	delete_array(&a, n);
	delete_array(&b, n);
	delete_array(&c1, n);
	delete_array(&c2, n);

	return 0;
}