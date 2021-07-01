/* gamma-wl.c -- Wayland gamma adjustment header
   This file is part of Redshift.

   Redshift is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Redshift is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Redshift.  If not, see <http://www.gnu.org/licenses/>.

   Copyright (c) 2015  Giulio Camuffo <giuliocamuffo@gmail.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <alloca.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <wayland-client.h>
#include <math.h>

#ifdef ENABLE_NLS
# include <libintl.h>
# define _(s) gettext(s)
#else
# define _(s) s
#endif

#include "os-compatibility.h"
#include "os-compatibility-impl.h"
//#include "colorramp.h"

#include "gamma-control-client-protocol.h"
#include "gamma-control-protocol.h"
#include "orbital-authorizer-client-protocol.h"
#include "orbital-authorizer-protocol.h"

/* Color setting */
typedef struct {
	int temperature;
	float gamma[3];
	float brightness;
} color_setting_t;

/* Whitepoint values for temperatures at 100K intervals.
   These will be interpolated for the actual temperature.
   This table was provided by Ingo Thies, 2013. See
   the file README-colorramp for more information. */
static const float blackbody_color[] = {
	1.00000000,  0.18172716,  0.00000000, /* 1000K */
	1.00000000,  0.25503671,  0.00000000, /* 1100K */
	1.00000000,  0.30942099,  0.00000000, /* 1200K */
	1.00000000,  0.35357379,  0.00000000, /* ...   */
	1.00000000,  0.39091524,  0.00000000,
	1.00000000,  0.42322816,  0.00000000,
	1.00000000,  0.45159884,  0.00000000,
	1.00000000,  0.47675916,  0.00000000,
	1.00000000,  0.49923747,  0.00000000,
	1.00000000,  0.51943421,  0.00000000,
	1.00000000,  0.54360078,  0.08679949,
	1.00000000,  0.56618736,  0.14065513,
	1.00000000,  0.58734976,  0.18362641,
	1.00000000,  0.60724493,  0.22137978,
	1.00000000,  0.62600248,  0.25591950,
	1.00000000,  0.64373109,  0.28819679,
	1.00000000,  0.66052319,  0.31873863,
	1.00000000,  0.67645822,  0.34786758,
	1.00000000,  0.69160518,  0.37579588,
	1.00000000,  0.70602449,  0.40267128,
	1.00000000,  0.71976951,  0.42860152,
	1.00000000,  0.73288760,  0.45366838,
	1.00000000,  0.74542112,  0.47793608,
	1.00000000,  0.75740814,  0.50145662,
	1.00000000,  0.76888303,  0.52427322,
	1.00000000,  0.77987699,  0.54642268,
	1.00000000,  0.79041843,  0.56793692,
	1.00000000,  0.80053332,  0.58884417,
	1.00000000,  0.81024551,  0.60916971,
	1.00000000,  0.81957693,  0.62893653,
	1.00000000,  0.82854786,  0.64816570,
	1.00000000,  0.83717703,  0.66687674,
	1.00000000,  0.84548188,  0.68508786,
	1.00000000,  0.85347859,  0.70281616,
	1.00000000,  0.86118227,  0.72007777,
	1.00000000,  0.86860704,  0.73688797,
	1.00000000,  0.87576611,  0.75326132,
	1.00000000,  0.88267187,  0.76921169,
	1.00000000,  0.88933596,  0.78475236,
	1.00000000,  0.89576933,  0.79989606,
	1.00000000,  0.90198230,  0.81465502,
	1.00000000,  0.90963069,  0.82838210,
	1.00000000,  0.91710889,  0.84190889,
	1.00000000,  0.92441842,  0.85523742,
	1.00000000,  0.93156127,  0.86836903,
	1.00000000,  0.93853986,  0.88130458,
	1.00000000,  0.94535695,  0.89404470,
	1.00000000,  0.95201559,  0.90658983,
	1.00000000,  0.95851906,  0.91894041,
	1.00000000,  0.96487079,  0.93109690,
	1.00000000,  0.97107439,  0.94305985,
	1.00000000,  0.97713351,  0.95482993,
	1.00000000,  0.98305189,  0.96640795,
	1.00000000,  0.98883326,  0.97779486,
	1.00000000,  0.99448139,  0.98899179,
	1.00000000,  1.00000000,  1.00000000, /* 6500K */
	0.98947904,  0.99348723,  1.00000000,
	0.97940448,  0.98722715,  1.00000000,
	0.96975025,  0.98120637,  1.00000000,
	0.96049223,  0.97541240,  1.00000000,
	0.95160805,  0.96983355,  1.00000000,
	0.94303638,  0.96443333,  1.00000000,
	0.93480451,  0.95923080,  1.00000000,
	0.92689056,  0.95421394,  1.00000000,
	0.91927697,  0.94937330,  1.00000000,
	0.91194747,  0.94470005,  1.00000000,
	0.90488690,  0.94018594,  1.00000000,
	0.89808115,  0.93582323,  1.00000000,
	0.89151710,  0.93160469,  1.00000000,
	0.88518247,  0.92752354,  1.00000000,
	0.87906581,  0.92357340,  1.00000000,
	0.87315640,  0.91974827,  1.00000000,
	0.86744421,  0.91604254,  1.00000000,
	0.86191983,  0.91245088,  1.00000000,
	0.85657444,  0.90896831,  1.00000000,
	0.85139976,  0.90559011,  1.00000000,
	0.84638799,  0.90231183,  1.00000000,
	0.84153180,  0.89912926,  1.00000000,
	0.83682430,  0.89603843,  1.00000000,
	0.83225897,  0.89303558,  1.00000000,
	0.82782969,  0.89011714,  1.00000000,
	0.82353066,  0.88727974,  1.00000000,
	0.81935641,  0.88452017,  1.00000000,
	0.81530175,  0.88183541,  1.00000000,
	0.81136180,  0.87922257,  1.00000000,
	0.80753191,  0.87667891,  1.00000000,
	0.80380769,  0.87420182,  1.00000000,
	0.80018497,  0.87178882,  1.00000000,
	0.79665980,  0.86943756,  1.00000000,
	0.79322843,  0.86714579,  1.00000000,
	0.78988728,  0.86491137,  1.00000000, /* 10000K */
	0.78663296,  0.86273225,  1.00000000,
	0.78346225,  0.86060650,  1.00000000,
	0.78037207,  0.85853224,  1.00000000,
	0.77735950,  0.85650771,  1.00000000,
	0.77442176,  0.85453121,  1.00000000,
	0.77155617,  0.85260112,  1.00000000,
	0.76876022,  0.85071588,  1.00000000,
	0.76603147,  0.84887402,  1.00000000,
	0.76336762,  0.84707411,  1.00000000,
	0.76076645,  0.84531479,  1.00000000,
	0.75822586,  0.84359476,  1.00000000,
	0.75574383,  0.84191277,  1.00000000,
	0.75331843,  0.84026762,  1.00000000,
	0.75094780,  0.83865816,  1.00000000,
	0.74863017,  0.83708329,  1.00000000,
	0.74636386,  0.83554194,  1.00000000,
	0.74414722,  0.83403311,  1.00000000,
	0.74197871,  0.83255582,  1.00000000,
	0.73985682,  0.83110912,  1.00000000,
	0.73778012,  0.82969211,  1.00000000,
	0.73574723,  0.82830393,  1.00000000,
	0.73375683,  0.82694373,  1.00000000,
	0.73180765,  0.82561071,  1.00000000,
	0.72989845,  0.82430410,  1.00000000,
	0.72802807,  0.82302316,  1.00000000,
	0.72619537,  0.82176715,  1.00000000,
	0.72439927,  0.82053539,  1.00000000,
	0.72263872,  0.81932722,  1.00000000,
	0.72091270,  0.81814197,  1.00000000,
	0.71922025,  0.81697905,  1.00000000,
	0.71756043,  0.81583783,  1.00000000,
	0.71593234,  0.81471775,  1.00000000,
	0.71433510,  0.81361825,  1.00000000,
	0.71276788,  0.81253878,  1.00000000,
	0.71122987,  0.81147883,  1.00000000,
	0.70972029,  0.81043789,  1.00000000,
	0.70823838,  0.80941546,  1.00000000,
	0.70678342,  0.80841109,  1.00000000,
	0.70535469,  0.80742432,  1.00000000,
	0.70395153,  0.80645469,  1.00000000,
	0.70257327,  0.80550180,  1.00000000,
	0.70121928,  0.80456522,  1.00000000,
	0.69988894,  0.80364455,  1.00000000,
	0.69858167,  0.80273941,  1.00000000,
	0.69729688,  0.80184943,  1.00000000,
	0.69603402,  0.80097423,  1.00000000,
	0.69479255,  0.80011347,  1.00000000,
	0.69357196,  0.79926681,  1.00000000,
	0.69237173,  0.79843391,  1.00000000,
	0.69119138,  0.79761446,  1.00000000, /* 15000K */
	0.69003044,  0.79680814,  1.00000000,
	0.68888844,  0.79601466,  1.00000000,
	0.68776494,  0.79523371,  1.00000000,
	0.68665951,  0.79446502,  1.00000000,
	0.68557173,  0.79370830,  1.00000000,
	0.68450119,  0.79296330,  1.00000000,
	0.68344751,  0.79222975,  1.00000000,
	0.68241029,  0.79150740,  1.00000000,
	0.68138918,  0.79079600,  1.00000000,
	0.68038380,  0.79009531,  1.00000000,
	0.67939381,  0.78940511,  1.00000000,
	0.67841888,  0.78872517,  1.00000000,
	0.67745866,  0.78805526,  1.00000000,
	0.67651284,  0.78739518,  1.00000000,
	0.67558112,  0.78674472,  1.00000000,
	0.67466317,  0.78610368,  1.00000000,
	0.67375872,  0.78547186,  1.00000000,
	0.67286748,  0.78484907,  1.00000000,
	0.67198916,  0.78423512,  1.00000000,
	0.67112350,  0.78362984,  1.00000000,
	0.67027024,  0.78303305,  1.00000000,
	0.66942911,  0.78244457,  1.00000000,
	0.66859988,  0.78186425,  1.00000000,
	0.66778228,  0.78129191,  1.00000000,
	0.66697610,  0.78072740,  1.00000000,
	0.66618110,  0.78017057,  1.00000000,
	0.66539706,  0.77962127,  1.00000000,
	0.66462376,  0.77907934,  1.00000000,
	0.66386098,  0.77854465,  1.00000000,
	0.66310852,  0.77801705,  1.00000000,
	0.66236618,  0.77749642,  1.00000000,
	0.66163375,  0.77698261,  1.00000000,
	0.66091106,  0.77647551,  1.00000000,
	0.66019791,  0.77597498,  1.00000000,
	0.65949412,  0.77548090,  1.00000000,
	0.65879952,  0.77499315,  1.00000000,
	0.65811392,  0.77451161,  1.00000000,
	0.65743716,  0.77403618,  1.00000000,
	0.65676908,  0.77356673,  1.00000000,
	0.65610952,  0.77310316,  1.00000000,
	0.65545831,  0.77264537,  1.00000000,
	0.65481530,  0.77219324,  1.00000000,
	0.65418036,  0.77174669,  1.00000000,
	0.65355332,  0.77130560,  1.00000000,
	0.65293404,  0.77086988,  1.00000000,
	0.65232240,  0.77043944,  1.00000000,
	0.65171824,  0.77001419,  1.00000000,
	0.65112144,  0.76959404,  1.00000000,
	0.65053187,  0.76917889,  1.00000000,
	0.64994941,  0.76876866,  1.00000000, /* 20000K */
	0.64937392,  0.76836326,  1.00000000,
	0.64880528,  0.76796263,  1.00000000,
	0.64824339,  0.76756666,  1.00000000,
	0.64768812,  0.76717529,  1.00000000,
	0.64713935,  0.76678844,  1.00000000,
	0.64659699,  0.76640603,  1.00000000,
	0.64606092,  0.76602798,  1.00000000,
	0.64553103,  0.76565424,  1.00000000,
	0.64500722,  0.76528472,  1.00000000,
	0.64448939,  0.76491935,  1.00000000,
	0.64397745,  0.76455808,  1.00000000,
	0.64347129,  0.76420082,  1.00000000,
	0.64297081,  0.76384753,  1.00000000,
	0.64247594,  0.76349813,  1.00000000,
	0.64198657,  0.76315256,  1.00000000,
	0.64150261,  0.76281076,  1.00000000,
	0.64102399,  0.76247267,  1.00000000,
	0.64055061,  0.76213824,  1.00000000,
	0.64008239,  0.76180740,  1.00000000,
	0.63961926,  0.76148010,  1.00000000,
	0.63916112,  0.76115628,  1.00000000,
	0.63870790,  0.76083590,  1.00000000,
	0.63825953,  0.76051890,  1.00000000,
	0.63781592,  0.76020522,  1.00000000,
	0.63737701,  0.75989482,  1.00000000,
	0.63694273,  0.75958764,  1.00000000,
	0.63651299,  0.75928365,  1.00000000,
	0.63608774,  0.75898278,  1.00000000,
	0.63566691,  0.75868499,  1.00000000,
	0.63525042,  0.75839025,  1.00000000,
	0.63483822,  0.75809849,  1.00000000,
	0.63443023,  0.75780969,  1.00000000,
	0.63402641,  0.75752379,  1.00000000,
	0.63362667,  0.75724075,  1.00000000,
	0.63323097,  0.75696053,  1.00000000,
	0.63283925,  0.75668310,  1.00000000,
	0.63245144,  0.75640840,  1.00000000,
	0.63206749,  0.75613641,  1.00000000,
	0.63168735,  0.75586707,  1.00000000,
	0.63131096,  0.75560036,  1.00000000,
	0.63093826,  0.75533624,  1.00000000,
	0.63056920,  0.75507467,  1.00000000,
	0.63020374,  0.75481562,  1.00000000,
	0.62984181,  0.75455904,  1.00000000,
	0.62948337,  0.75430491,  1.00000000,
	0.62912838,  0.75405319,  1.00000000,
	0.62877678,  0.75380385,  1.00000000,
	0.62842852,  0.75355685,  1.00000000,
	0.62808356,  0.75331217,  1.00000000,
	0.62774186,  0.75306977,  1.00000000, /* 25000K */
	0.62740336,  0.75282962,  1.00000000  /* 25100K */
};


static void
interpolate_color(float a, const float *c1, const float *c2, float *c)
{
	c[0] = (1.0-a)*c1[0] + a*c2[0];
	c[1] = (1.0-a)*c1[1] + a*c2[1];
	c[2] = (1.0-a)*c1[2] + a*c2[2];
}

/* Helper macro used in the fill functions */
#define F(Y, C)  pow((Y) * setting->brightness * \
		     white_point[C], 1.0/setting->gamma[C])

void
colorramp_fill(uint16_t *gamma_r, uint16_t *gamma_g, uint16_t *gamma_b,
	       int size, const color_setting_t *setting)
{
	/* Approximate white point */
	float white_point[3];
	float alpha = (setting->temperature % 100) / 100.0;
	int temp_index = ((setting->temperature - 1000) / 100)*3;
	interpolate_color(alpha, &blackbody_color[temp_index],
			  &blackbody_color[temp_index+3], white_point);

	for (int i = 0; i < size; i++) {
		gamma_r[i] = F((double)gamma_r[i]/(UINT16_MAX+1), 0) *
			(UINT16_MAX+1);
		gamma_g[i] = F((double)gamma_g[i]/(UINT16_MAX+1), 1) *
			(UINT16_MAX+1);
		gamma_b[i] = F((double)gamma_b[i]/(UINT16_MAX+1), 2) *
			(UINT16_MAX+1);
	}
}

typedef struct {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_callback *callback;
	uint32_t gamma_control_manager_id;
	struct zwlr_gamma_control_manager_v1 *gamma_control_manager;
	int num_outputs;
	struct output *outputs;
	int authorized;
} wayland_state_t;

struct output {
	uint32_t global_id;
	struct wl_output *output;
	struct zwlr_gamma_control_v1 *gamma_control;
	uint32_t gamma_size;
};

static int
wayland_init(wayland_state_t **state)
{
	/* Initialize state. */
	*state = malloc(sizeof(**state));
	if (*state == NULL) return -1;

	memset(*state, 0, sizeof **state);
	return 0;
}

static void
authorizer_feedback_granted(void *data, struct orbital_authorizer_feedback *feedback)
{
	wayland_state_t *state = data;
	state->authorized = 1;
}

static void
authorizer_feedback_denied(void *data, struct orbital_authorizer_feedback *feedback)
{
	fprintf(stderr, _("Fatal: redshift was not authorized to bind the 'zwlr_gamma_control_manager_v1' interface.\n"));
	exit(EXIT_FAILURE);
}

static const struct orbital_authorizer_feedback_listener authorizer_feedback_listener = {
	authorizer_feedback_granted,
	authorizer_feedback_denied
};

static void
registry_global(void *data, struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version)
{
	wayland_state_t *state = data;

	if (strcmp(interface, "zwlr_gamma_control_manager_v1") == 0) {
		state->gamma_control_manager_id = id;
		state->gamma_control_manager = wl_registry_bind(registry, id, &zwlr_gamma_control_manager_v1_interface, 1);
	} else if (strcmp(interface, "wl_output") == 0) {
		state->num_outputs++;
		if (!(state->outputs = realloc(state->outputs, state->num_outputs * sizeof(struct output)))) {
			fprintf(stderr, _("Failed to allcate memory\n"));
			return;
		}

		struct output *output = &state->outputs[state->num_outputs - 1];
		output->global_id = id;
		output->output = wl_registry_bind(registry, id, &wl_output_interface, 1);
		output->gamma_control = NULL;
	} else if (strcmp(interface, "orbital_authorizer") == 0) {
		struct wl_event_queue *queue = wl_display_create_queue(state->display);

		struct orbital_authorizer *auth = wl_registry_bind(registry, id, &orbital_authorizer_interface, 1u);
		wl_proxy_set_queue((struct wl_proxy *)auth, queue);

		struct orbital_authorizer_feedback *feedback = orbital_authorizer_authorize(auth, "zwlr_gamma_control_manager_v1");
		orbital_authorizer_feedback_add_listener(feedback, &authorizer_feedback_listener, state);

		int ret = 0;
		while (!state->authorized && ret >= 0) {
			ret = wl_display_dispatch_queue(state->display, queue);
		}

		orbital_authorizer_feedback_destroy(feedback);
		orbital_authorizer_destroy(auth);
		wl_event_queue_destroy(queue);
	}
}

static void
registry_global_remove(void *data, struct wl_registry *registry, uint32_t id)
{
	wayland_state_t *state = data;

	if (state->gamma_control_manager_id == id) {
		fprintf(stderr, _("The zwlr_gamma_control_manager_v1 was removed\n"));
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i < state->num_outputs; ++i) {
		struct output *output = &state->outputs[i];
		if (output->global_id == id) {
			if (output->gamma_control) {
				zwlr_gamma_control_v1_destroy(output->gamma_control);
				output->gamma_control = NULL;
			}
			wl_output_destroy(output->output);

			/* If the removed output is not the last one in the array move the last one
			 * in the now empty slot. Then shrink the array */
			if (i < --state->num_outputs) {
				memcpy(output, &state->outputs[state->num_outputs], sizeof(struct output));
			}
			state->outputs = realloc(state->outputs, state->num_outputs * sizeof(struct output));

			return;
		}
	}
}

static const struct wl_registry_listener registry_listener = {
	registry_global,
	registry_global_remove
};

static void
gamma_control_gamma_size(void *data, struct zwlr_gamma_control_v1 *control, uint32_t size)
{
	struct output *output = data;
	output->gamma_size = size;
}

static void
gamma_control_failed(void *data, struct zwlr_gamma_control_v1 *control)
{
}


static const struct zwlr_gamma_control_v1_listener gamma_control_listener = {
	gamma_control_gamma_size,
	gamma_control_failed
};

static int
wayland_start(wayland_state_t *state)
{
	state->display = wl_display_connect(NULL);
	if (!state->display) {
		fputs(_("Could not connect to wayland display, exiting.\n"), stderr);
		return -1;
	}
	state->registry = wl_display_get_registry(state->display);

	wl_registry_add_listener(state->registry, &registry_listener, state);

	wl_display_roundtrip(state->display);
	if (!state->gamma_control_manager) {
		return -1;
	}
	if (state->num_outputs > 0 && !state->outputs) {
		return -1;
	}

	return 0;
}

static void
wayland_restore(wayland_state_t *state)
{
	for (int i = 0; i < state->num_outputs; ++i) {
		struct output *output = &state->outputs[i];
		if (output->gamma_control) {
			zwlr_gamma_control_v1_destroy(output->gamma_control);
			output->gamma_control = NULL;
		}
	}
	wl_display_flush(state->display);
}

static void
wayland_free(wayland_state_t *state)
{
	int ret = 0;
	/* Wait for the sync callback to destroy everything, otherwise
	 * we could destroy the gamma control before gamma has been set */
	while (state->callback && ret >= 0) {
		ret = wl_display_dispatch(state->display);
	}
	if (state->callback) {
		fprintf(stderr, _("Ignoring error on wayland connection while waiting to disconnect: %d\n"), ret);
		wl_callback_destroy(state->callback);
	}

	for (int i = 0; i < state->num_outputs; ++i) {
		struct output *output = &state->outputs[i];
		if (output->gamma_control) {
			zwlr_gamma_control_v1_destroy(output->gamma_control);
			output->gamma_control = NULL;
		}
		wl_output_destroy(output->output);
	}

	if (state->gamma_control_manager) {
		zwlr_gamma_control_manager_v1_destroy(state->gamma_control_manager);
	}
	if (state->registry) {
		wl_registry_destroy(state->registry);
	}
	if (state->display) {
		wl_display_disconnect(state->display);
	}

	free(state);
}

static void
callback_done(void *data, struct wl_callback *cb, uint32_t t)
{
	wayland_state_t *state = data;
	state->callback = NULL;
	wl_callback_destroy(cb);
}

static const struct wl_callback_listener callback_listener = {
	callback_done
};

static int
wayland_set_temperature(wayland_state_t *state, const color_setting_t *setting)
{
	int ret = 0, roundtrip = 0;

	/* We wait for the sync callback to throttle a bit and not send more
	 * requests than the compositor can manage, otherwise we'd get disconnected.
	 * This also allows us to dispatch other incoming events such as
	 * wl_registry.global_remove. */
	while (state->callback && ret >= 0) {
		ret = wl_display_dispatch(state->display);
	}
	if (ret < 0) {
		fprintf(stderr, _("The Wayland connection experienced a fatal error: %d\n"), ret);
		return ret;
	}

	for (int i = 0; i < state->num_outputs; ++i) {
		struct output *output = &state->outputs[i];
		if (!output->gamma_control) {
			output->gamma_control = zwlr_gamma_control_manager_v1_get_gamma_control(state->gamma_control_manager, output->output);
			zwlr_gamma_control_v1_add_listener(output->gamma_control, &gamma_control_listener, output);
			roundtrip = 1;
		}
	}
	if (roundtrip) {
		wl_display_roundtrip(state->display);
	}

	for (int i = 0; i < state->num_outputs; ++i) {
		struct output *output = &state->outputs[i];
		int size = output->gamma_size;
		size_t ramp_bytes = size * sizeof(uint16_t);
		size_t total_bytes = ramp_bytes * 3;

		int fd = os_create_anonymous_file(total_bytes);
		if (fd < 0) {
			perror("os_create_anonymous_file");
			return -1;
		}

		void *ptr = mmap(NULL, total_bytes,
			PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (ptr == MAP_FAILED) {
			perror("mmap");
			close(fd);
			return -1;
		}

		uint16_t *r_gamma = ptr;
		uint16_t *g_gamma = ptr + ramp_bytes;
		uint16_t *b_gamma = ptr + 2 * ramp_bytes;

		/* Initialize gamma ramps to pure state */
		for (int i = 0; i < size; i++) {
			uint16_t value = (double)i / size * (UINT16_MAX+1);
			r_gamma[i] = value;
			g_gamma[i] = value;
			b_gamma[i] = value;
		}

		colorramp_fill(r_gamma, g_gamma, b_gamma, size, setting);
		if (munmap(ptr, total_bytes) == -1) {
			perror("munmap");
			close(fd);
			return -1;
		}

		zwlr_gamma_control_v1_set_gamma(output->gamma_control, fd);
		close(fd);
	}

	state->callback = wl_display_sync(state->display);
	wl_callback_add_listener(state->callback, &callback_listener, state);
	wl_display_flush(state->display);

	return 0;
}
