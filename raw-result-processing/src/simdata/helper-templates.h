#ifndef HELPER_TEMPLATES_H
#define HELPER_TEMPLATES_H

#include <iostream>
#include <vector>

template <typename T>
double CalcAvg(const std::vector<T> &values)
{
  if (values.size() == 0)
    return 0;
  double sum = 0;
  for (auto it = values.begin(); it != values.end(); it++)
  {
    sum += *it;
  }
  return sum / values.size();
}

template <typename T>
double CalcSTD(const std::vector<T> &values)
{
  if (values.size() == 0)
    return 0;
  double avg = CalcAvg(values);
  double sum = 0;
  for (auto it = values.begin(); it != values.end(); it++)
  {
    sum += (*it - avg) * (*it - avg);
  }
  return sqrt(sum / values.size());
}

#endif  // HELPER_TEMPLATES_H