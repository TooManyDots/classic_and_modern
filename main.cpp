#define _CRT_SECURE_NO_WARNINGS 1

#include <cstdlib>
#include <cstdio>
#include <chrono>
#include <vector>
#include <unordered_set>
#include <atlstr.h>

using std::vector;
using std::unordered_set;

struct WindowsGuiContainer {
  vector<int> data;
  int GetItemData(int index) const { return data[index]; }
};

struct MiningSite {
  int nNummer;
  CString sRegion;
};

enum class RegionSelect : int { Nothing_Selected = 0, Without_A_Region, Region_Selected };

static vector<int> classic(const WindowsGuiContainer& m_lbMiningSite, const vector<MiningSite> m_vecMiningSites, const vector<int>& vecSelectedIndices, RegionSelect eRegionSelect, const CString& sSelectedRegion);
static vector<int> modern(const WindowsGuiContainer& m_lbMiningSite, const vector<MiningSite> m_vecMiningSites, const vector<int>& vecSelectedIndices, RegionSelect eRegionSelect, const CString& sSelectedRegion);
static bool same(vector<int> a, vector<int> b);

struct Trial {
  vector<MiningSite> mining_sites;
  WindowsGuiContainer container;
  vector<int> selected_indices;
  unsigned int seed;
  RegionSelect region_select;
  CString region;

  vector<int> result_classic;
  vector<int> result_modern;

  std::chrono::steady_clock::time_point start_classic;
  std::chrono::steady_clock::time_point start_modern;
  std::chrono::steady_clock::time_point end_classic;
  std::chrono::steady_clock::time_point end_modern;
  

  long long duration_us_classic() const {
    return std::chrono::duration_cast<std::chrono::microseconds>(end_classic - start_classic).count();
  }
  long long duration_us_modern() const {
    return std::chrono::duration_cast<std::chrono::microseconds>(end_modern - start_modern).count();
  }
};

static int generate_random_unique_number(unordered_set<int>& already_used, int max);
static vector<Trial> generate_trials(const vector<MiningSite>& all_mining_sites, long trial_count, int max_mining_site_count, int max_selected_indices_count);

static vector<MiningSite> generate_all_mining_sites();


static int rand_at_least_one(int max) {
  int result = 0;
  while ( result == 0) {
    result = rand() % max;
  }
  return result;
}

int main() {
  const long trial_count = 10000;
  const int max_mining_site_count = 2000; // 18_000 or so is max, unless you add a word_alpha file with more words
  const int max_selected_indices_count = 200;

  printf("Generating base data.\n");
  srand(time(0));
  vector<MiningSite> all_mining_sites = generate_all_mining_sites();
  if (all_mining_sites.size() == 0) return 1;


  printf("Generating trials.\n");
  vector<Trial> trials = generate_trials(all_mining_sites, trial_count, max_mining_site_count, max_selected_indices_count);

  printf("Starting classic.\n");
  // classic runs
  for(auto& trial: trials) {
    srand(trial.seed);
    trial.start_classic = std::chrono::steady_clock::now();
    auto result = classic(trial.container, trial.mining_sites, trial.selected_indices, trial.region_select, trial.region);
    trial.end_classic = std::chrono::steady_clock::now();
    MemoryBarrier();
    trial.result_classic = std::move(result);
  }

  printf("Starting modern.\n");
  // modern runs
  for(auto& trial: trials) {
    srand(trial.seed);
    trial.start_modern = std::chrono::steady_clock::now();
    auto result = modern(trial.container, trial.mining_sites, trial.selected_indices, trial.region_select, trial.region);
    trial.end_modern = std::chrono::steady_clock::now();
    MemoryBarrier();
    trial.result_modern = std::move(result);
  }

  {
    long long total = 0;
    for (const auto& trial: trials) {
      auto duration = trial.duration_us_classic();
      total += duration;
//      printf("%lld ns\n", duration);
    }
    printf("Classic total: %lld microseconds. Per run: %.0f microseconds.\n", total, (double)total / (double)trials.size());
  }

  {
    long long total = 0;
    for (const auto& trial: trials) {
      auto duration = trial.duration_us_modern();
      total += duration;
//      printf("%lld ns\n", duration);
    }
    printf(" Modern total: %lld microseconds. Per run: %.0f microseconds.\n", total, (double)total / (double)trials.size());
    for(const auto& trial: trials) {
      if ( !same(trial.result_classic, trial.result_modern) ) {
        printf("Different result. Count %zd vs %zd\n", trial.result_classic.size(), trial.result_modern.size());
      }
    }
  }
}



static vector<Trial> generate_trials(const vector<MiningSite>& all_mining_sites, long trial_count, int max_mining_site_count, int max_selected_indices_count) {
  vector<Trial> result;
  for (long trial_index = 0; trial_index < trial_count; ++trial_index) {

    Trial trial;
    trial.seed = rand();
    int mining_site_count = rand_at_least_one(max_mining_site_count);
    int mining_site_start = rand() % all_mining_sites.size();
    if (mining_site_start + mining_site_count >= all_mining_sites.size()) // make sure we are not trying to go past the end
      mining_site_start -= mining_site_count;

    trial.mining_sites = vector<MiningSite>(all_mining_sites.begin() + mining_site_start, all_mining_sites.begin() + mining_site_start + mining_site_count);
    for (const auto& mining_site : trial.mining_sites)
      trial.container.data.push_back({ mining_site.nNummer });

    int selected_indices_count = rand() % min(max_selected_indices_count, mining_site_count);
    unordered_set<int> already_used_indices;
    for (int i = 0; i < selected_indices_count; ++i) {
      int index = generate_random_unique_number(already_used_indices, mining_site_count);
      trial.selected_indices.push_back(index);
    }

    trial.region_select = (RegionSelect)(rand() % 3);
    if (trial.region_select == RegionSelect::Region_Selected) {
      trial.region = trial.mining_sites[rand() % trial.mining_sites.size()].sRegion;
    }

    result.push_back(trial);
  }
  return result;
}

static vector<MiningSite> generate_all_mining_sites() {
  vector<MiningSite> result;
  unordered_set<int> all_mining_sites_ids;
  FILE* f = fopen("words_alpha.txt", "rb");
  if (!f) {
    printf("Could not open words file.\n");
    return result;
  }
  char line[1024];
  while ( true ) {
    memset(line, 0, 1024);
    char* l = fgets(line, 1024, f);
    if (!l) break;
    int n = generate_random_unique_number(all_mining_sites_ids, INT16_MAX);
    if ( rand() % 10 == 0) // 10% chance for a site to have no region
      result.push_back({ n, ""});
    else
      result.push_back({ n, line});
  }
  fclose(f);
  return result;
}

static int generate_random_unique_number(unordered_set<int>& already_used, int max) {
  while ( true ) {
    int n = rand() % max;
    if ( already_used.find(n) == already_used.end())
    {
      already_used.insert(n);
      return n;
    }
  }
}

static vector<int> classic(const WindowsGuiContainer& m_lbMiningSite, const vector<MiningSite> m_vecMiningSites, const vector<int>& vecSelectedIndices, RegionSelect eRegionSelect, const CString& sSelectedRegion) {
  vector<int> vecMiningSiteNrs;

  // now ask each selected entry in the list-box for what it's mining-site-id ( the value stored in GetItemData )
  for (const int nSelectedIndex: vecSelectedIndices)
  {
    const int nMiningSiteNr = m_lbMiningSite.GetItemData(nSelectedIndex);

    switch (eRegionSelect)
    {
      case RegionSelect::Nothing_Selected:
      {
        // if no region is selected, then we always want the element to be selected
        vecMiningSiteNrs.push_back(nMiningSiteNr);
        break;
      }

      case RegionSelect::Without_A_Region:
      {
        // Only miningsites without a region should be retained.
        for (const auto& miningSite : m_vecMiningSites)
        {
          if (miningSite.nNummer != nMiningSiteNr) continue;
          if (miningSite.sRegion.IsEmpty())
          {
            vecMiningSiteNrs.push_back(nMiningSiteNr);
            break;
          }
        }
        break;
      }

      case(RegionSelect::Region_Selected):
      {
        // check if the mining site is within the region
        for (const auto& miningSite : m_vecMiningSites)
        {
          if (miningSite.nNummer != nMiningSiteNr) continue;
          if (miningSite.sRegion == sSelectedRegion)
          {
            vecMiningSiteNrs.push_back(nMiningSiteNr);
            break;
          }
        }
        break;
      }
    }
  }

  return vecMiningSiteNrs;
}

#include <algorithm>
#include <iterator>
#include <ranges>

static vector<int> modern(const WindowsGuiContainer& m_lbMiningSite, const vector<MiningSite> m_vecMiningSites, const vector<int>& vecSelectedIndices, RegionSelect eRegionSelect, const CString& sSelectedRegion) {
  auto selectedMiningSiteNrs = vecSelectedIndices | std::views::transform([&](int i) { return m_lbMiningSite.GetItemData(i); });
  auto matchingNrs = m_vecMiningSites
    | std::views::filter([&](MiningSite x) { return std::ranges::any_of(selectedMiningSiteNrs, [&](int num) { return num == x.nNummer; }); })
    | std::views::filter([&](MiningSite x)
      {
        switch (eRegionSelect)
        {
          case RegionSelect::Nothing_Selected: return true;
          case RegionSelect::Without_A_Region: return x.sRegion.IsEmpty();
          case RegionSelect::Region_Selected: return x.sRegion == sSelectedRegion;
        }
      })
    | std::views::transform([](MiningSite x) { return x.nNummer; });
    // | std::ranges::to<std::vector<int>>(); // C++ 23
  return { matchingNrs.begin(), matchingNrs.end() };
}

static bool same(vector<int> a, vector<int> b) {
  std::sort(a.begin(), a.end());
  std::sort(b.begin(), b.end());
  return a == b;
}
