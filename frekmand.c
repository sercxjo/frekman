#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sensors/sensors.h>
#include <cpufreq.h>

int count_cpus()
{
  FILE *fp;
  char value[10];
  unsigned int ret= 0;
  unsigned int cpunr= 0;

  fp= fopen("/proc/stat", "r");
  if(!fp) {
    //Couldn't count the number of CPUs (%s: %s), assuming 256
    return 256;
  }

  while (!feof(fp)) {
    if (!fgets(value, sizeof value, fp))
      continue;
    value[sizeof value - 1]= '\0';
    if (strlen(value) < (sizeof value - 2))
      continue;
    if (strstr(value, "cpu "))
      continue;
    if (sscanf(value, "cpu%d ", &cpunr) != 1)
      continue;
    if (cpunr > ret)
      ret= cpunr;
  }
  fclose(fp);

  /* cpu count starts from 0, on error return 1 (UP) */
  return (ret+1);
}

int main(int argc, char **argv)
{
  sensors_init(NULL);

  int ncpu = count_cpus();
  double Tmax= 70, //< begin of braking
    Tcrit= 75  , //< maximum braking
    T0=0, T1=0;    //< memory for previous temperatures

  for (;;) {
    //printf("\033[H\033[2J\033[3J");

    // Get CPU temperatures
    double T= 0;
    int nT= 0;
    const sensors_chip_name *chip;
    for (int chip_nr= 0; nT <= ncpu && (chip= sensors_get_detected_chips(NULL, &chip_nr));) {
      const sensors_feature *feature;
      double t= 0;
      for (int i= 0; (feature= sensors_get_features(chip, &i));) {
        char *label;
        if (!(label= sensors_get_label(chip, feature))) continue;
        if (feature->type == SENSORS_FEATURE_TEMP && strcmp(label, "CPU") == 0 || strncmp(label, "Core ", 5) == 0) {
          const sensors_subfeature *sf;
          if ((sf= sensors_get_subfeature(chip, feature, SENSORS_SUBFEATURE_TEMP_INPUT)) && !sensors_get_value(chip, sf->number, &t)) {
            T+= t;
            ++nT;
          }
        }
        free(label);
        if (t > Tcrit) break;
      }
      if (t > Tcrit) {
        T= t;
        nT= 1;
        break;
      }
    }
    if (!nT) {
      fprintf(stderr, "No CPU temperature sensors found\n");
      return 1;
    }
    T/= nT;

    double t= T;
    if (T1 > t) t= (t + T1)/2;
    if (T0 > t) t= (t + T0)/2;
    T0= T1;
    T1= T;

    // Set new frequences
    double f=  t >= Tcrit ? 0 : t <= Tmax ? 1 : 1 - (t - Tmax)/(Tcrit - Tmax);
    for (unsigned int cpu=0; cpu < ncpu; cpu++) {
      unsigned long min= 1, max= 0;
      if (cpufreq_get_hardware_limits(cpu, &min, &max) || min >= max) continue;
      struct cpufreq_policy *policy= cpufreq_get_policy(cpu);
      if (!policy)  continue;

      struct cpufreq_policy new_pol= {
       .min= min,
       .max= min + (max - min)*f*f,
       .governor= "powersave",
      };

      printf("CPU%3d [%4.1f %9lu .. %9lu .. %9lu %9lu %9lu] - %s\n",
           cpu, T, policy->min, cpufreq_get_freq_kernel(cpu), new_pol.max, policy->max, max, policy->governor);
      cpufreq_put_policy(policy);
      cpufreq_set_policy(cpu, &new_pol);
    }

    usleep(t >= (Tmax + Tcrit)/2? 250000 : 1000000);
  }
}
