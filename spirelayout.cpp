#include <cctype>
#include <iostream>
#include <iomanip>
#include "spirelayout.h"

using namespace std;

TrapUpgrades::TrapUpgrades():
	fire(1),
	frost(1),
	poison(1),
	lightning(1)
{ }

TrapUpgrades::TrapUpgrades(const string &upgrades)
{
	if(upgrades.size()!=4)
		throw invalid_argument("TrapUpgrades::TrapUpgrades");
	for(auto c: upgrades)
		if(!isdigit(c))
			throw invalid_argument("TrapUpgrades::TrapUpgrades");

	fire = upgrades[0]-'0';
	frost = upgrades[1]-'0';
	poison = upgrades[2]-'0';
	lightning = upgrades[3]-'0';
}

string TrapUpgrades::str() const
{
	char buf[4];
	buf[0] = '0'+fire;
	buf[1] = '0'+frost;
	buf[2] = '0'+poison;
	buf[3] = '0'+lightning;
	return string(buf, 4);
}


TrapEffects::TrapEffects(const TrapUpgrades &upgrades):
	fire_damage(50),
	frost_damage(10),
	chill_dur(3),
	poison_damage(5),
	lightning_damage(50),
	shock_dur(1),
	damage_multi(2),
	special_multi(2)
{
	if(upgrades.fire>=2)
		fire_damage *= 10;
	if(upgrades.fire>=3)
		fire_damage *= 5;
	if(upgrades.fire>=4)
		fire_damage *= 2;
	if(upgrades.fire>=5)
		fire_damage *= 2;
	if(upgrades.fire>=6)
		fire_damage *= 10;
	if(upgrades.fire>=7)
		fire_damage *= 10;
	if(upgrades.fire>=8)
		fire_damage *= 100;

	if(upgrades.frost>=2)
	{
		++chill_dur;
		frost_damage *= 5;
	}
	if(upgrades.frost>=3)
		frost_damage *= 10;
	if(upgrades.frost>=4)
		frost_damage *= 5;
	if(upgrades.frost>=5)
		frost_damage *= 2;
	if(upgrades.frost>=6)
	{
		++chill_dur;
		frost_damage *= 5;
	}

	if(upgrades.poison>=2)
		poison_damage *= 2;
	if(upgrades.poison>=4)
		poison_damage *= 2;
	if(upgrades.poison>=5)
		poison_damage *= 2;
	if(upgrades.poison>=6)
		poison_damage *= 2;
	if(upgrades.poison>=7)
		poison_damage *= 2;

	if(upgrades.lightning>=2)
	{
		lightning_damage *= 10;
		++shock_dur;
	}
	if(upgrades.lightning>=3)
	{
		lightning_damage *= 10;
		damage_multi *= 2;
	}
	if(upgrades.lightning>=5)
	{
		lightning_damage *= 10;
		++shock_dur;
	}
	if(upgrades.lightning>=6)
	{
		lightning_damage *= 10;
		damage_multi *= 2;
	}
}


Step::Step():
	trap(0),
	slow(0),
	shock(false),
	kill_pct(0),
	toxic_pct(0),
	rs_bonus(0),
	direct_damage(0),
	toxicity(0)
{ }


const char Layout::traps[] = "_FZPLSCK";

Layout::Layout():
	damage(0),
	cost(0),
	rs_per_sec(0),
	threat(0),
	cycle(0)
{ }

void Layout::build_steps(vector<Step> &steps) const
{
	unsigned cells = data.size();
	steps.clear();
	steps.reserve(cells*3);

	uint8_t column_flags[5] = { };
	for(unsigned i=0; i<cells; ++i)
		if(data[i]=='L')
			++column_flags[i%5];

	vector<uint16_t> floor_flags(cells/5, 0);
	for(unsigned i=0; i<cells; ++i)
	{
		unsigned j = i/5;
		char t = data[i];
		if(t=='F')
			floor_flags[j] += 1+0x10*column_flags[i%5];
		else if(t=='S')
			floor_flags[j] |= 0x08;
	}

	TrapEffects effects(upgrades);

	unsigned chilled = 0;
	unsigned frozen = 0;
	unsigned shocked = 0;
	unsigned damage_multi = 1;
	unsigned special_multi = 1;
	unsigned repeat = 1;
	for(unsigned i=0; i<cells; )
	{
		char t = data[i];
		Step step;
		step.cell = i;
		step.trap = t;

		if(t=='Z')
		{
			step.direct_damage = effects.frost_damage*damage_multi;
			chilled = effects.chill_dur*special_multi+1;
			frozen = 0;
			repeat = 1;
		}
		else if(t=='F')
		{
			step.direct_damage = effects.fire_damage*damage_multi;
			if(floor_flags[i/5]&0x08)
				step.direct_damage *= 2;
			if(chilled && upgrades.frost>=3)
				step.direct_damage = step.direct_damage*5/4;
			if(upgrades.lightning>=4)
				step.direct_damage = step.direct_damage*(10+column_flags[i%5])/10;
			if(upgrades.fire>=4)
				step.kill_pct = 20;
		}
		else if(t=='P')
		{
			step.toxicity = effects.poison_damage*damage_multi;
			if(upgrades.frost>=4 && i+1<cells && data[i+1]=='Z')
				step.toxicity *= 4;
			if(upgrades.poison>=3)
			{
				if(i>0 && data[i-1]=='P')
					step.toxicity *= 3;
				if(i+1<cells && data[i+1]=='P')
					step.toxicity *= 3;
			}
			if(upgrades.lightning>=4)
				step.toxicity = step.toxicity*(10+column_flags[i%5])/10;
		}
		else if(t=='L')
		{
			step.direct_damage = effects.lightning_damage*damage_multi;
			shocked = effects.shock_dur+1;
			damage_multi = effects.damage_multi;
			special_multi = effects.special_multi;
		}
		else if(t=='S')
		{
			uint16_t flags = floor_flags[i/5];
			step.direct_damage = effects.fire_damage*(flags&0x07);
			if(upgrades.lightning>=4)
				step.direct_damage += effects.fire_damage*(flags>>4)/10;
			step.direct_damage *= 2*damage_multi;
			if(chilled && upgrades.frost>=3)
				step.direct_damage = step.direct_damage*5/4;
		}
		else if(t=='K')
		{
			if(chilled)
			{
				chilled = 0;
				frozen = 5*special_multi+1;
			}
			repeat = 1;
		}
		else if(t=='C')
			step.toxic_pct = 25*special_multi;

		step.slow = (frozen ? 2 : chilled ? 1 : 0);
		step.shock = (shocked!=0);
		if(repeat>1 && upgrades.frost>=5)
			step.rs_bonus = 2;
		steps.push_back(step);

		if(shocked && !--shocked)
		{
			damage_multi = 1;
			special_multi = 1;
		}

		if(repeat && --repeat)
			continue;

		++i;
		if(chilled)
			--chilled;
		if(frozen)
			--frozen;
		repeat = (frozen ? 3 : chilled ? 2 : 1);
	}
}

Layout::SimResult Layout::simulate(const vector<Step> &steps, uint64_t max_hp, bool debug) const
{
	SimResult result;
	result.max_hp = max_hp;

	if(debug && max_hp)
		cout << "Enemy HP: " << max_hp << endl;

	unsigned last_cell = 0;
	unsigned repeat = 0;
	uint64_t kill_damage = 0;
	uint64_t toxicity = 0;
	unsigned rs_pct = 100;
	for(const auto &s: steps)
	{
		result.damage += s.direct_damage;
		kill_damage = max(kill_damage, result.damage*100/(100-s.kill_pct));
		if(upgrades.poison>=5 && max_hp && result.damage*4>=max_hp)
			toxicity += s.toxicity*5;
		else
			toxicity += s.toxicity;
		toxicity = toxicity*(100+s.toxic_pct)/100;
		result.damage += toxicity;
		rs_pct += s.rs_bonus;
		kill_damage = max(kill_damage, result.damage);

		if(result.kill_cell<0)
		{
			++result.steps_taken;
			if(kill_damage>=max_hp)
			{
				result.kill_cell = s.cell;
				result.runestone_pct = rs_pct;
				if(upgrades.fire>=7 && s.trap=='F')
					result.runestone_pct = result.runestone_pct*6/5;
			}
		}

		if(debug)
		{
			repeat = (s.cell==last_cell ? repeat+1 : 0);

			cout << setw(2) << s.cell << ':' << repeat << ": " << s.trap << ' ' << setw(9) << result.damage;
			if(max_hp && upgrades.poison>=5)
			{
				if(result.damage>max_hp)
					cout << "  0%";
				else if(result.damage)
					cout << ' ' << setw(2) << (max_hp-result.damage)*100/max_hp << '%';
				else
					cout << " **%";
			}
			if(toxicity)
				cout << " P" << setw(6) << toxicity;
			else
				cout << "        ";
			cout << ' ' << (s.slow==1 ? 'C' : ' ') << (s.slow==2 ? 'F' : ' ') << (s.shock ? 'S' : ' ') << endl;
		}

		last_cell = s.cell;
	}

	if(debug)
		cout << "Kill damage: " << kill_damage << endl;

	result.toxicity = toxicity;
	if(kill_damage>=max_hp)
		result.damage = kill_damage;

	return result;
}

void Layout::build_results(const vector<Step> &steps, unsigned subdiv, vector<SimResult> &results) const
{
	results.clear();
	results.reserve(subdiv);
	uint64_t max_hp = simulate(steps, 0).damage;
	if(upgrades.poison>=5)
		max_hp = simulate(steps, max_hp).damage;
	max_hp = max_hp*3/2;

	for(unsigned i=1; i<=subdiv; ++i)
		results.push_back(simulate(steps, max_hp*i/subdiv));
}

Layout::SimResult Layout::interpolate_result(const vector<SimResult> &results, uint64_t hp) const
{
	unsigned max_hp = results.back().max_hp;
	if(hp>=max_hp)
		return results.back();

	unsigned i = hp*(results.size()-1)/max_hp;
	const SimResult &low = results[i];
	const SimResult &high = results[i+1];
	uint64_t delta = high.max_hp-low.max_hp;
	uint64_t t = i-low.max_hp;

	SimResult result;
	result.max_hp = hp;
	result.damage = low.max_hp+(high.max_hp-low.max_hp)*t/delta;

	return result;
}

template<typename F>
unsigned Layout::integrate_results_for_threat(const vector<SimResult> &results, unsigned thrt, const F &func) const
{
	if(results.empty())
		return 0;

	unsigned range = min(max<int>(0.53*thrt, 150), 850);
	uint64_t max_hp = 10+4*thrt+pow(1.012, thrt);
	uint64_t min_hp = max_hp*(1000-range)/1000;

	auto i = results.begin();
	const SimResult *p = &*i++;

	unsigned p_wt = 0;
	unsigned total_wt = 0;
	for(; (i!=results.end() && p->max_hp<max_hp); ++i)
	{
		if(i->max_hp<min_hp)
		{
			p = &*i;
			continue;
		}

		uint64_t d_hp = i->max_hp-p->max_hp;
		unsigned weight = 0;
		int split = -1;
		if(i->max_hp>damage && p->max_hp<damage)
			split = ((damage-p->max_hp)*2000/d_hp+1)/2;

		if(i->max_hp>max_hp)
		{
			unsigned t = ((max_hp-p->max_hp)*2000/d_hp+1)/2;
			if(split>=0)
			{
				weight = t-min<int>(t, split);
				p_wt += t-weight;
			}
			else
			{
				weight = t*t/2000;
				p_wt += t-weight;
			}
		}
		else if(p->max_hp<min_hp)
		{
			unsigned t = ((i->max_hp-min_hp)*2000/d_hp+1)/2;
			if(split>=0)
			{
				weight = min<int>(1000-split, t);
				p_wt += t-weight;
			}
			else
			{
				p_wt = t*t/2000;
				weight = t-p_wt;
			}
		}
		else if(split>=0)
		{
			p_wt += 1000-split;
			weight = split;
		}
		else
		{
			p_wt += 500;
			weight = 500;
		}

		if(p_wt)
		{
			func(*p, p_wt);
			total_wt += p_wt;
		}

		p = &*i;
		p_wt = weight;
	}

	if(p_wt)
	{
		func(*p, p_wt);
		total_wt += p_wt;
	}

	return total_wt;
}

void Layout::update(UpdateMode mode)
{
	update_cost();

	vector<Step> steps;
	build_steps(steps);
	if(mode==FAST || mode==COMPATIBLE)
		update_damage(steps);
	if(mode!=FAST)
	{
		vector<SimResult> results;
		build_results(steps, 40, results);
		if(mode!=COMPATIBLE)
			update_damage(steps, results);
		update_threat(results);
		update_runestones(results);
	}
}

void Layout::update_damage(const vector<Step> &steps)
{
	damage = simulate(steps, 0).damage;
	if(upgrades.poison>=5)
	{
		uint64_t high_damage = simulate(steps, damage).damage;
		refine_damage(steps, damage, high_damage);
	}
}

void Layout::update_damage(const vector<Step> &steps, const vector<SimResult> &results)
{
	damage = results[0].damage;
	if(upgrades.poison>=5)
	{
		unsigned i;
		for(i=0; (i<results.size() && results[i].kill_cell>=0); ++i) ;
		if(i<results.size())
			refine_damage(steps, results[i].damage, damage);
	}
}

void Layout::refine_damage(const vector<Step> &steps, uint64_t low, uint64_t high)
{
	for(unsigned i=0; (i<10 && low*101<high*100); ++i)
	{
		uint64_t mid = (low+high*3)/4;
		damage = simulate(steps, mid).damage;
		if(damage>=mid)
			low = mid;
		else
		{
			high = mid;
			low = damage;
		}
	}

	damage = low;
}

void Layout::update_cost()
{
	uint64_t fire_cost = 100;
	uint64_t frost_cost = 100;
	uint64_t poison_cost = 500;
	uint64_t lightning_cost = 1000;
	uint64_t strength_cost = 3000;
	uint64_t condenser_cost = 6000;
	uint64_t knowledge_cost = 9000;
	cost = 0;
	for(char t: data)
	{
		uint64_t prev_cost = cost;
		if(t=='F')
		{
			cost += fire_cost;
			fire_cost = fire_cost*3/2;
		}
		else if(t=='Z')
		{
			cost += frost_cost;
			frost_cost *= 5;
		}
		else if(t=='P')
		{
			cost += poison_cost;
			poison_cost = poison_cost*7/4;
		}
		else if(t=='L')
		{
			cost += lightning_cost;
			lightning_cost *= 3;
		}
		else if(t=='S')
		{
			cost += strength_cost;
			strength_cost *= 100;
		}
		else if(t=='C')
		{
			cost += condenser_cost;
			condenser_cost *= 100;
		}
		else if(t=='K')
		{
			cost += knowledge_cost;
			knowledge_cost *= 100;
		}

		if(cost<prev_cost)
		{
			cost = numeric_limits<uint64_t>::max();
			break;
		}
	}
}

void Layout::update_threat(const vector<SimResult> &results)
{
	unsigned cells = data.size();
	unsigned floors = cells/5;

	static double log_base = log(1.012);
	unsigned low = log(damage-4*log(damage)/log_base)/log_base;
	unsigned high = low+64;

	for(unsigned i=0; i<6; ++i)
	{
		threat = (low+high+1)/2;

		int change = 0;
		integrate_results_for_threat(results, threat, [&change, cells, floors](const SimResult &r, unsigned w)
		{
			if(r.kill_cell>=0)
				change += (cells-r.kill_cell+4)/5*w;
			else
				change -= floors*ceil((r.max_hp-r.damage)/(r.max_hp*0.15))*w;
		});

		if(change>0)
			low = threat;
		else
			high = threat;
	}
}

void Layout::update_runestones(const vector<SimResult> &results)
{
	unsigned capacity = (1+(data.size()+1)/2)*3;
	uint64_t runestones = 0;

	unsigned total_w = integrate_results_for_threat(results, threat, [this, &runestones, capacity](const SimResult &r, unsigned w)
	{
		uint64_t rs_gain = 0;
		if(r.damage>=r.max_hp)
		{
			rs_gain = ((r.max_hp+599)/600+threat/20)*pow(1.00116, threat);
			rs_gain = rs_gain*r.runestone_pct/100;
		}
		else if(upgrades.poison>=6)
			rs_gain = r.toxicity/10;
		runestones += rs_gain*w*capacity/max(r.steps_taken, capacity);
	});

	rs_per_sec = runestones/max(total_w*3, 1U);
}

void Layout::cross_from(const Layout &other, Random &random)
{
	unsigned cells = min(data.size(), other.data.size());
	for(unsigned i=0; i<cells; ++i)
		if(random()&1)
			data[i] = other.data[i];
}

void Layout::mutate(unsigned mode, unsigned count, Random &random)
{
	unsigned cells = data.size();
	unsigned locality = (cells>=10 ? random()%(cells*2/15) : 0);
	unsigned base = 0;
	if(locality)
	{
		cells -= locality*5;
		base = (random()%locality)*5;
	}

	unsigned traps_count = (upgrades.lightning>0 ? 7 : 5);
	for(unsigned i=0; i<count; ++i)
	{
		unsigned op = 0;  // replace only
		if(mode==1)       // permute only
			op = 1+random()%4;
		else if(mode==2)  // any operation
			op = random()%8;

		unsigned t = 1+random()%traps_count;
		if(!upgrades.lightning && t>=4)
			++t;
		char trap = traps[t];

		if(op==0)  // replace
			data[base+random()%cells] = trap;
		else if(op==1 || op==2 || op==5)  // swap, rotate, insert
		{
			unsigned pos = base+random()%cells;
			unsigned end = base+random()%(cells-1);
			while(end==pos)
				++end;

			if(op==1)
				swap(data[pos], data[end]);
			else
			{
				if(op==2)
					trap = data[end];

				for(unsigned j=end; j>pos; --j)
					data[j] = data[j-1];
				for(unsigned j=end; j<pos; ++j)
					data[j] = data[j+1];
				data[pos] = trap;
			}
		}
		else if(cells>=10)  // floor operations
		{
			unsigned floors = cells/5;
			unsigned pos = random()%floors;
			unsigned end = random()%(floors-1);
			while(end==pos)
				++end;

			pos = base+pos*5;
			end = base+end*5;

			if(op==3 || op==6)  // rotate, duplicate
			{
				char floor[5];
				for(unsigned j=0; j<5; ++j)
					floor[j] = data[end+j];

				for(unsigned j=end; j>pos; --j)
					data[j+4] = data[j-1];
				for(unsigned j=end; j<pos; ++j)
					data[j] = data[j+5];

				if(op==3)
				{
					for(unsigned j=0; j<5; ++j)
						data[pos+j] = floor[j];
				}
			}
			else if(op==7)  // copy
			{
				for(unsigned j=0; j<5; ++j)
					data[end+j] = data[pos+j];
			}
			else if(op==4)  // swap
			{
				for(unsigned j=0; j<5; ++j)
					swap(data[pos+j], data[end+j]);
			}
		}
	}
}

bool Layout::is_valid() const
{
	unsigned cells = data.size();
	bool have_strength = false;
	for(unsigned i=0; i<cells; ++i)
	{
		if(i%5==0)
			have_strength = false;
		if(data[i]=='S')
		{
			if(have_strength)
				return false;
			have_strength = true;
		}
	}

	return true;
}


Layout::SimResult::SimResult():
	max_hp(0),
	damage(0),
	runestone_pct(100),
	steps_taken(0),
	kill_cell(-1)
{ }
