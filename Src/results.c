/* $Id$ */
/*
 * results.c - Routines for generating analysis result variables.
 *
 *      Donald J. Dovey
 *      Lawrence Livermore National Laboratory
 *      Mar 19 1992
 *
 ************************************************************************
 * Modifications:
 *  I. R. Corey - Aug 26, 2005: Add option to hide result from pull-down
 *                list.
 *                See SRC# 339.
 *
 *  I. R. Corey - Oct 1, 2005: Added two new derived results: fnmass &
 *                fnvol.
 *                See SRC# 365.
 *
 *  I. R. Corey - Jan 31, 2007: Added capability for Griz to display int
 *                and long results.
 *                See SRC# 434.
 *
 *  I.R. Corey -  May 1, 2008:  Added new derived nodal result = dispr,
 *                radial nodal displacement.
 *                See SRC#: 532
 ************************************************************************
 */

#include <stdlib.h>
#include "viewer.h"
#ifdef GRIZ_SERVER_BUILD
#include "cJSON.h"
#endif

#define OK 0

static Bool_type
mod_required( Result *, Result_modifier_type, int, int );


static char *node_disp_shorts_xy[] =
{
    "dispx", "dispy", NULL
};
static char *node_disp_longs_xy[] =
{
    "X Displacement", "Y Displacement", NULL
};

static char *node_disp_longs_r[] =
{
    "XY Radial Displacement", NULL
};

static char *node_disp_shorts_z[] =
{
    "dispz", NULL
};
static char *node_disp_shorts_r[] =
{
    "dispr", NULL
};
static char *node_disp_longs_z[] =
{
    "Z Displacement", NULL
};

static char *node_disp_mag_shorts[] =
{
    "dispmag", NULL
};
static char *node_modedisp_mag_shorts[] =
{
    "evec_disp_mag", NULL
};
static char *node_moderot_mag_shorts[] =
{
    "evec_rot_mag", NULL
};
static char *node_disp_mag_longs[] =
{
    "Displacement Magnitude", NULL
};
static char *node_modedisp_mag_longs[] =
{
    "Disp Eigenvector Magnitude", NULL
};
static char *node_moderot_mag_longs[] =
{
    "Rot Eigenvector Magnitude", NULL
};

static char *node_modedisp_primals[] =
{
    "evec_disp", NULL
};
static char *node_moderot_primals[] =
{
    "evec_rot", NULL
};

static char *node_disp_primals[] =
{
    "nodpos", NULL
};

static char *node_vel_shorts_xy[] =
{
    "velx", "vely", NULL
};
static char *node_vel_longs_xy[] =
{
    "X Velocity", "Y Velocity", NULL
};

static char *node_vel_shorts_z[] =
{
    "velz", NULL
};
static char *node_vel_longs_z[] =
{
    "Z Velocity", NULL
};

static char *node_vel_mag_shorts[] =
{
    "velmag", NULL
};
static char *node_vel_mag_longs[] =
{
    "Velocity Magnitude", NULL
};

static char *node_vel_primals1[] =
{
    "nodvel", NULL
};

static char *node_vel_primals2[] =
{
    "nodpos", NULL
};

static int node_vel_primal_sclasses[] =
{
    G_NODE
};

static char *node_rot_vel_mag_shorts[] =
{
    "rotvelmag", NULL
};

static char *node_rot_vel_mag_longs[] =
{
    "Rotational Velocity Magnitude", NULL
};

static char *node_rot_vel_mag_primals1[] =
{
    "rotvel", NULL
};

static char *node_acc_shorts_xy[] =
{
    "accx", "accy", NULL
};
static char *node_acc_longs_xy[] =
{
    "X Acceleration", "Y Acceleration", NULL
};

static char *node_acc_shorts_z[] =
{
    "accz", NULL
};
static char *node_acc_longs_z[] =
{
    "Z Acceleration", NULL
};

static char *node_acc_mag_shorts[] =
{
    "accmag", NULL
};
static char *node_acc_mag_longs[] =
{
    "Acceleration Magnitude", NULL
};

static char *node_acc_primals1[] =
{
    "nodacc", NULL
};
static char *node_acc_primals2[] =
{
    "nodpos", NULL
};


static char *node_temp_short[] =
{
    "temp", NULL
};
static char *node_temp_long[] =
{
    "Temperature", NULL
};
static char *node_temp_primal[] =
{
    "temp", NULL
};


static char *node_pint_short[] =
{
    "pint", NULL
};
static char *node_pint_long[] =
{
    "Pressure Intensity", NULL
};
static char *node_pint_primal[] =
{
    "press", NULL
};


static char *node_helicity_short[] =
{
    "hel", NULL
};
static char *node_helicity_long[] =
{
    "Helicity", NULL
};
static char *node_helicity_primal[] =
{
    "nodvel", "nodvort", NULL
};


static char *node_enstrophy_short[] =
{
    "ens", NULL
};
static char *node_enstrophy_long[] =
{
    "Enstrophy", NULL
};
static char *node_enstrophy_primal[] =
{
    "nodvort", NULL
};


static char *node_pvmag_short[] =
{
    "pvmag", NULL
};
static char *node_pvmag_long[] =
{
    "Projected Vector Magnitude", NULL
};
static char *node_pvmag_primal[] =
{
    NULL
};


static char *beam_sae_short[] =
{
    "saep", "saem", NULL
};
static char *beam_sae_long[] =
{
    "S Axial Strian (+)", "S Axial Strain (-)", NULL
};
static char *beam_sae_primal[] =
{
    "axfor", "smom", NULL
};


static char *beam_tae_short[] =
{
    "taep", "taem", NULL
};
static char *beam_tae_long[] =
{
    "T Axial Strain (+)", "T Axial Strain (-)", NULL
};
static char *beam_tae_primal[] =
{
    "axfor", "tmom", NULL
};


static char *shell_stress_shorts[] =
{
    "sx", "sy", "sz", "sxy", "syz", "szx", NULL
};
static char *shell_stress_longs[] =
{
    "X Stress", "Y Stress", "Z Stress", "XY Stress", "YZ Stress", "ZX Stress",
    NULL
};
static char *shell_stress_primals[] =
{
    "stress_in", "stress_mid", "stress_out", NULL
};

static char *shell_surf_shorts[] =
{
    "surf1", "surf2", "surf3", "surf4", "surf5", "surf6", "eff1", "eff2",
    "effmax", NULL
};
static char *shell_surf_longs[] =
{
    "Surface Stress 1", "Surface Stress 2", "Surface Stress 3",
    "Surface Stress 4", "Surface Stress 5", "Surface Stress 6",
    "Effective Upper Surface Stress", "Effective Lower Surface Stress",
    "Maximum Effective Surface Stress", NULL
};
static char *shell_surf_primals[] =
{
    "bend", "shear", "normal", "thick", NULL
};


static char *shell_strain_shorts[] =
{
    "ex", "ey", "ez", "exy", "eyz", "ezx", "gamxy", "gamyz", "gamzx", NULL
};
static char *shell_strain_longs[] =
{
    "X Strain", "Y Strain", "Z Strain", "XY Strain", "YZ Strain", "ZX Strain",
    "XY Engr Strain", "YZ Engr Strain", "ZX Engr Strain", NULL
};
static char *shell_strain_primals[] =
{
    "strain_in", "strain_out", NULL
};


static char *shell_eeff_shorts[] =
{
    "eeff", NULL
};
static char *shell_eeff_longs[] =
{
    "Eff Plastic Strain", NULL
};
/*
 * With all three primals listed here, if we don't have them all, "eeff" won't
 * show up as a derived result.  This is probably OK, since historically it's
 * always been all-or-nothing anyway.  If we broke this into three
 * Result_candidate entries, we could avoid the all-or-nothing issue, but
 * then compute_shell_eff_strain() would always have to check the
 * Result_candidate primal against the current setting of analy->ref_surf
 * (the new check_func function could be used for this).  Another alternative
 * would be to have surface-specific short names (i.e., "eeff_mid", etc.),
 * but then we'd lose "eeff" result sharing on meshes with both hex's and
 * shells generating "eeff" data.
 */
static char *shell_eeff_primals[] =
{
    "eeff_mid", "eeff_in", "eeff_out", NULL
};


static char *hex_stress_shorts[] =
{
    "sx", "sy", "sz", "sxy", "syz", "szx", NULL
};
static char *hex_stress_longs[] =
{
    "X Stress", "Y Stress", "Z Stress", "XY Stress", "YZ Stress", "ZX Stress",
    NULL
};
static char *hex_stress_primals[] =
{
    "stress", NULL
};


static char *pressure_shorts[] =
{
    "press", NULL
};
static char *pressure_longs[] =
{
    "Pressure", NULL
};

static char *effective_stress_shorts[] =
{
    "seff", NULL
};
static char *effective_stress_longs[] =
{
    "Effective Stress", NULL
};

static char *principal_stress_shorts[] =
{
    "pdev1", "pdev2", "pdev3", "maxshr", "prin1", "prin2", "prin3", NULL
};
static char *principal_stress_longs[] =
{
    "Prin Dev Stress 1", "Prin Dev Stress 2", "Prin Dev Stress 3",
    "Maximum Shear Stress", "Principal Stress 1", "Principal Stress 2",
    "Principal Stress 3", NULL
};
static char *stress_invariant_primals[] =
{
    "stress", NULL
};

static char *strain_invariant_one_shorts[] =
{
    "etrace", NULL
};
static char *strain_invariant_one_longs[] =
{
    "Strain Trace - Volumetric Strain", NULL
};

static char *strain_invariant_two_shorts[] =
{
    "strinv2", NULL
};
static char *strain_invariant_two_longs[] =
{
    "2nd Invariant of Strain", NULL
};

static char *principal_strain_shorts[] =
{
    "pdstrn1", "pdstrn2", "pdstrn3", "pshrstr", "pstrn1", "pstrn2", "pstrn3",
    "gamxy", "gamyz", "gamzx", NULL
};
static char *principal_strain_longs[] =
{
    "Prin Dev Strain 1", "Prin Dev Strain 2", "Prin Dev Strain 3",
    "Max Tensor Shear Strain", "Principal Strain 1", "Principal Strain 2",
    "Principal Strain 3", "XY Engr Strain", "YZ Engr Strain", "ZX Engr Strain", NULL
};
static char *strain_invariant_primals[] =
{
    "strain", NULL
};


static char *hex_strain_shorts[] =
{
    "ex", "ey", "ez", "exy", "eyz", "ezx", "pdstrn1", "pdstrn2", "pdstrn3",
    "pshrstr", "pstrn1", "pstrn2", "pstrn3", "gamxy", "gamyz", "gamzx", NULL
};
static char *hex_strain_longs[] =
{
    "X Strain", "Y Strain", "Z Strain", "XY Strain", "YZ Strain", "ZX Strain",
    "Prin Dev Strain 1", "Prin Dev Strain 2", "Prin Dev Strain 3",
    "Max Tensor Shear Strain", "Principal Strain 1", "Principal Strain 2",
    "Principal Strain 3", "XY Engr Strain", "YZ Engr Strain", "ZX Engr Strain",
    NULL
};
/*
 * Problem - If strain type is "RATE", compute_hex_strain() attempts to read
 * node velocities.  The current design/use of the Result_candidate does not
 * permit encoding a dependency like this.  If "nodvel" were included in
 * hex_strain_primals below, we would lose all the strain results when/if
 * nodal velocities weren't available.  So, we're ignoring the dependency
 * on velocity here and adding checks for it in compute_hex_strain() to
 * err off with an INFO_DIALOG if strain type is "RATE" and nodal velocity
 * isn't available.  If we had explicit strain rate derived results, we
 * could put the rate calc in its own derived result routine with its own
 * Result_candidate entry here, but this would this bypass the "switch
 * <strain type>" model in Griz.
 */
static char *hex_strain_th_primals[] =
{
    "hx", NULL
};

static char *hex_strain_primals[] =
{
    "nodpos", NULL
};


static char *hex_eeff_shorts[] =
{
    "eeff", NULL
};
static char *hex_eeff_longs[] =
{
    "Eff Plastic Strain", NULL
};
static char *hex_eeff_primals[] =
{
    "eeff", NULL
};


static char *hex_vol_shorts[] =
{
    "relvol", "evol", NULL
};
static char *hex_vol_longs[] =
{
    "Relative Volume", "Logarithmic Volumetric Strain", NULL
};
static char *hex_vol_primals[] =
{
    "nodpos", NULL
};


static char *hex_damage_shorts[] =
{
    "damage", NULL
};
static char *hex_damage_longs[] =
{
    "Damage", NULL
};
static char *hex_damage_primals[] =
{
    "nplot", NULL
};
static char *hex_damage_primals1[] =
{
    "eeff", NULL
};


/* Free-Node Mass Show variables */
static char *fnmass_shorts[] =
{
    "fnmass", NULL
};
static char *fnmass_longs[] =
{
    "Free Node Mass", NULL
};


static char *anmass_shorts[] =
{
    "anmass", NULL
};
static char *anmass_longs[] =
{
    "Nodal Mass", NULL
};


static char *fnmom_shorts[] =
{
    "fnmom", NULL
};
static char *fnmom_longs[] =
{
    "Nodal Momemtum", NULL
};


static char *fnvol_shorts[] =
{
    "fnvol", NULL
};
static char *fnvol_longs[] =
{
    "Free Node Volume", NULL
};
static char *fnmass_primals[] =
{
    "nodpos", NULL
};

Result_candidate possible_results[] =
{
    /*
     * For nodal displacement, velocity, and acceleration, the
     * z-components have separate Result_candidate's which are
     * initialized as 3D quantities only.  This organization precludes
     * their occurrence for 2D datasets, whereas the x- and y-components
     * are allowed for both 2D and 3D datasets.
     */
    {
        // Can calculate for G_NODE
        { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, 1 },
        { 0, 0, 1, 0, 0, 0, 1, 0, 0 },
        FALSE,
        FALSE,
        compute_node_displacement,
        NULL,
        "Node Displacement",
        node_disp_shorts_xy,
        node_disp_longs_xy,
        node_disp_primals,
        QTY_SCLASS
    },

    {
        // Can calculate for G_NODE
        { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 0, 1, 0, 0 },
        FALSE,
        FALSE,
        compute_node_displacement,
        NULL,
        "Node Displacement",
        node_disp_shorts_z,
        node_disp_longs_z,
        node_disp_primals,
        QTY_SCLASS
    },

    {
        // Can calculate for G_NODE
        { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 0, 1, 0, 0 },
        FALSE,
        FALSE,
        compute_node_radial_displacement,
        NULL,
        NULL,
        node_disp_shorts_r,
        node_disp_longs_r,
        node_disp_primals,
        QTY_SCLASS
    },
    {
        // Can calculate for G_NODE
        { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, 1 },
        { 0, 0, 1, 0, 0, 0, 1, 0, 0 },
        FALSE,
        FALSE,
        compute_node_displacement_mag,
        NULL,
        NULL,
        node_disp_mag_shorts,
        node_disp_mag_longs,
        node_disp_primals,
        QTY_SCLASS
    },

    {
        // Can calculate for G_NODE
        { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, 1 },
        { 0, 0, 1, 0, 0, 0, 1, 0, 0 },
        FALSE,
        FALSE,
        compute_node_modaldisplacement_mag,
        NULL,
        NULL,
        node_modedisp_mag_shorts,
        node_modedisp_mag_longs,
        node_modedisp_primals,
        QTY_SCLASS
    },

    {
        // Can calculate for G_NODE
        { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, 1 },
        { 0, 0, 1, 0, 0, 0, 1, 0, 0 },
        FALSE,
        FALSE,
        compute_node_modaldisplacement_mag,
        NULL,
        NULL,
        node_moderot_mag_shorts,
        node_moderot_mag_longs,
        node_moderot_primals,
        QTY_SCLASS
    },
    /*
     * Node velocity can occur explicitly in the data or be derived from
     * displacements, so there are two sets of Result_candidate's for it.
     * The logic in io_wrappers.c which evaluates the candidates (see
     * create_derived_results()) will only allow a derived result to be
     * created once per subrecord, taking the first occurrence found here
     * which is supported by the subrecord over any subsequent ones.
     * The organization here will favor velocity as an explicit primal
     * result but will allow it to occur as derived from displacements
     * if velocities don't exist.
     */
    {
        // Can calculate for G_NODE
        { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, 1 },
        { 0, 0, 1, 0, 0, 0, 1, 0, 0 },
        TRUE,
        FALSE,
        compute_node_velocity,
        NULL,
        "Node Velocity",
        node_vel_shorts_xy,
        node_vel_longs_xy,
        node_vel_primals1,
        QTY_SCLASS
    },

    {
        // Can calculate for G_NODE
        { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, 1 },
        { 0, 0, 1, 0, 0, 0, 1, 0, 0 },
        TRUE,
        FALSE,
        compute_node_velocity,
        NULL,
        "Node Velocity",
        node_vel_shorts_xy,
        node_vel_longs_xy,
        node_vel_primals2,
        QTY_SCLASS
    },

    {
        // Can calculate for G_NODE
        { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 0, 1, 0, 0 },
        TRUE,
        FALSE,
        compute_node_velocity,
        NULL,
        "Node Velocity",
        node_vel_shorts_z,
        node_vel_longs_z,
        node_vel_primals1,
        QTY_SCLASS
    },

    {
        // Can calculate for G_NODE
        { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 0, 1, 0, 0 },
        TRUE,
        FALSE,
        compute_node_velocity,
        NULL,
        "Node Velocity",
        node_vel_shorts_z,
        node_vel_longs_z,
        node_vel_primals2,
        QTY_SCLASS
    },

    {
        // Can calculate for G_NODE
        { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, 1 },
        { 0, 0, 1, 0, 0, 0, 1, 0, 0 },
        TRUE,
        FALSE,
        compute_node_velocity,
        NULL,
        NULL,
        node_vel_mag_shorts,
        node_vel_mag_longs,
        node_vel_primals1,
        QTY_SCLASS
    },

    {
        // Can calculate for G_NODE
        { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, 1 },
        { 0, 0, 1, 0, 0, 0, 1, 0, 0 },
        TRUE,
        FALSE,
        compute_node_velocity,
        NULL,
        NULL,
        node_vel_mag_shorts,
        node_vel_mag_longs,
        node_vel_primals2,
        QTY_SCLASS
    },

    {
        // Can calculate for G_NODE
        { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, 1 },
        { 0, 0, 1, 0, 0, 0, 1, 0, 0 },
        TRUE,
        FALSE,
        compute_node_velocity,
        NULL,
        NULL,
        node_rot_vel_mag_shorts,
        node_rot_vel_mag_longs,
        node_rot_vel_mag_primals1,
        QTY_SCLASS
    },
    /*
     * Ditto the comment above about velocities for accelerations.
     */
    {
        // Can calculate for G_NODE
        { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, 1 },
        { 0, 0, 1, 0, 0, 0, 1, 0, 0 },
        TRUE,
        FALSE,
        compute_node_acceleration,
        NULL,
        "Node Acceleration",
        node_acc_shorts_xy,
        node_acc_longs_xy,
        node_acc_primals1,
        QTY_SCLASS
    },

    {
        // Can calculate for G_NODE
        { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, 1 },
        { 0, 0, 1, 0, 0, 0, 1, 0, 0 },
        TRUE,
        FALSE,
        compute_node_acceleration,
        NULL,
        "Node Acceleration",
        node_acc_shorts_xy,
        node_acc_longs_xy,
        node_acc_primals2,
        QTY_SCLASS
    },

    {
        // Can calculate for G_NODE
        { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 0, 1, 0, 0 },
        TRUE,
        FALSE,
        compute_node_acceleration,
        NULL,
        "Node Acceleration",
        node_acc_shorts_z,
        node_acc_longs_z,
        node_acc_primals1,
        QTY_SCLASS
    },

    {
        // Can calculate for G_NODE
        { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 0, 1, 0, 0 },
        TRUE,
        FALSE,
        compute_node_acceleration,
        NULL,
        "Node Acceleration",
        node_acc_shorts_z,
        node_acc_longs_z,
        node_acc_primals2,
        QTY_SCLASS
    },

    {
        // Can calculate for G_NODE
        { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, 1 },
        { 0, 0, 1, 0, 0, 0, 1, 0, 0 },
        TRUE,
        FALSE,
        compute_node_acceleration,
        NULL,
        NULL,
        node_acc_mag_shorts,
        node_acc_mag_longs,
        node_acc_primals1,
        QTY_SCLASS
    },

    {
        // Can calculate for G_NODE
        { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, 1 },
        { 0, 0, 1, 0, 0, 0, 1, 0, 0 },
        TRUE,
        FALSE,
        compute_node_acceleration,
        NULL,
        NULL,
        node_acc_mag_shorts,
        node_acc_mag_longs,
        node_acc_primals2,
        QTY_SCLASS
    },

    {
        // Can calculate for G_NODE
        { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, 1 },
        { 0, 0, 1, 0, 0, 0, 1, 0, 0 },
        TRUE,
        FALSE,
        compute_node_pr_intense,
        NULL,
        NULL,
        node_pint_short,
        node_pint_long,
        node_pint_primal,
        QTY_SCLASS
    },

    {
        // Can calculate for G_NODE
        { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, 1 },
        { 0, 0, 1, 0, 0, 0, 1, 0, 0 },
        TRUE,
        FALSE,
        compute_node_helicity,
        NULL,
        NULL,
        node_helicity_short,
        node_helicity_long,
        node_helicity_primal,
        QTY_SCLASS
    },

    {
        // Can calculate for G_NODE
        { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, 1 },
        { 0, 0, 1, 0, 0, 0, 1, 0, 0 },
        TRUE,
        TRUE,
        compute_node_enstrophy,
        NULL,
        NULL,
        node_enstrophy_short,
        node_enstrophy_long,
        node_enstrophy_primal,
        QTY_SCLASS
    },
    {
        // Can calculate for G_NODE
        { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, 1 },
        { 0, 0, 1, 0, 0, 0, 1, 0, 0 },
        TRUE,
        FALSE,
        compute_vector_component,
        check_compute_vector_component,
        NULL,
        node_pvmag_short,
        node_pvmag_long,
        node_pvmag_primal,
        QTY_SCLASS
    },
    {
        // Can calculate for G_TRUSS and G_BEAM
        { 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 1, 0, 0, 0 },
        TRUE,
        FALSE,
        compute_beam_axial_strain,
        NULL,
        NULL,
        beam_sae_short,
        beam_sae_long,
        beam_sae_primal,
        QTY_SCLASS
    },

    {
        // Can calculate for G_TRUSS and G_BEAM
        { 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 1, 0, 0, 0 },
        TRUE,
        FALSE,
        compute_beam_axial_strain,
        NULL,
        NULL,
        beam_tae_short,
        beam_tae_long,
        beam_tae_primal,
        QTY_SCLASS
    },
    {
        // Can calculate for G_HEX
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 1, 0, 0, 0 },
        TRUE,
        FALSE,
        compute_hex_stress_wrapper,
        NULL,
        "Stress",
        hex_stress_shorts,
        hex_stress_longs,
        hex_stress_primals,
        QTY_SCLASS
    },

    {
        // Can calculate for G_TRUSS, G_BEAM, G_TRI, G_QUAD, G_TET, G_WEDGE, G_PYRAMID, G_HEX, G_PARTICLE
        { 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 1, 0, 0, 0 },
        TRUE,
        FALSE,
        compute_effective_stress,
        NULL,
        NULL,
        effective_stress_shorts,
        effective_stress_longs,
        stress_invariant_primals,
        QTY_SCLASS
    },

    {
        // Can calculate for G_TRUSS, G_BEAM, G_TRI, G_QUAD, G_TET, G_WEDGE, G_PYRAMID, G_HEX, G_PARTICLE
        { 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 1, 0, 0, 0 },
        TRUE,
        FALSE,
        compute_pressure,
        NULL,
        NULL,
        pressure_shorts,
        pressure_longs,
        stress_invariant_primals,
        QTY_SCLASS
    },

    {
        // Can calculate for G_TRUSS, G_BEAM, G_TRI, G_QUAD, G_TET, G_WEDGE, G_PYRAMID, G_HEX, G_PARTICLE
        { 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 1, 0, 0, 0 },
        TRUE,
        FALSE,
        compute_principal_stress,
        NULL,
        NULL,
        principal_stress_shorts,
        principal_stress_longs,
        stress_invariant_primals,
        QTY_SCLASS
    },

    {
        // Can calculate for G_TRUSS, G_BEAM, G_TRI, G_QUAD, G_TET, G_WEDGE, G_PYRAMID, G_HEX, G_PARTICLE
        { 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 1, 0, 0, 0 },
        TRUE,
        FALSE,
        compute_strain_invariant_one,
        NULL,
        NULL,
        strain_invariant_one_shorts,
        strain_invariant_one_longs,
        strain_invariant_primals,
        QTY_SCLASS
    },

    {
        // Can calculate for G_TRUSS, G_BEAM, G_TRI, G_QUAD, G_TET, G_WEDGE, G_PYRAMID, G_HEX, G_PARTICLE
        { 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 1, 0, 0, 0 },
        TRUE,
        FALSE,
        compute_strain_invariant_two,
        NULL,
        NULL,
        strain_invariant_two_shorts,
        strain_invariant_two_longs,
        strain_invariant_primals,
        QTY_SCLASS
    },

    {
        // Can calculate for G_TRUSS, G_BEAM, G_TRI, G_QUAD, G_TET, G_WEDGE, G_PYRAMID, G_HEX, G_PARTICLE
        { 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 1, 0, 0, 0 },
        TRUE,
        FALSE,
        compute_principal_strain,
        NULL,
        NULL,
        principal_strain_shorts,
        principal_strain_longs,
        strain_invariant_primals,
        QTY_SCLASS
    },

    {
        // Can calculate for G_HEX
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 1, 0, 0, 0 },
        TRUE,
        FALSE,
        compute_hex_strain,
        NULL,
        NULL,
        hex_strain_shorts,
        hex_strain_longs,
        hex_strain_primals,
        G_NODE
    },

    {
        // Can calculate for G_HEX
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 1, 0, 0, 0 },
        TRUE,
        FALSE,
        compute_hex_strain,
        NULL,
        NULL,
        hex_strain_shorts,
        hex_strain_longs,
        hex_strain_th_primals,
        G_NODE
    },

    {
        // Can calculate for G_HEX
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 1, 0, 0, 0 },
        TRUE,
        FALSE,
        compute_hex_eff_strain,
        NULL,
        NULL,
        hex_eeff_shorts,
        hex_eeff_longs,
        hex_eeff_primals,
        QTY_SCLASS
    },

    {
        // Can calculate for G_HEX
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 1, 0, 0, 0 },
        FALSE,
        FALSE,
        compute_hex_relative_volume,
        NULL,
        NULL,
        hex_vol_shorts,
        hex_vol_longs,
        hex_vol_primals,
        G_NODE
    },

    {
        // Can calculate for G_HEX
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 1, 0, 0, 0 },
        FALSE,
        FALSE,
        compute_hex_damage,
        NULL,
        NULL,
        hex_damage_shorts,
        hex_damage_longs,
        hex_damage_primals,
        QTY_SCLASS
    },

    {
        // Can calculate for G_HEX
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 1, 0, 0, 0 },
        FALSE,
        FALSE,
        compute_hex_damage,
        NULL,
        NULL,
        hex_damage_shorts,
        hex_damage_longs,
        hex_damage_primals1,
        QTY_SCLASS
    },

    {
        // Can calculate for G_TRI, G_QUAD
        { 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 1, 0, 0, 0 },
        TRUE,
        FALSE,
        compute_shell_effstress,
        NULL,
        NULL,
        effective_stress_shorts,
        effective_stress_longs,
        shell_stress_primals,
        QTY_SCLASS
    },

    {
        // Can calculate for G_TRI, G_QUAD
        { 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 1, 0, 0, 0 },
        TRUE,
        FALSE,
        compute_shell_press,
        NULL,
        NULL,
        pressure_shorts,
        pressure_longs,
        shell_stress_primals,
        QTY_SCLASS
    },

    {
        // Can calculate for G_TRI, G_QUAD
        { 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 1, 0, 0, 0 },
        TRUE,
        FALSE,
        compute_shell_principal_stress,
        NULL,
        NULL,
        principal_stress_shorts,
        principal_stress_longs,
        shell_stress_primals,
        QTY_SCLASS
    },

    {
        // Can calculate for G_TRI, G_QUAD
        { 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 1, 0, 0, 0 },
        TRUE,
        FALSE,
        compute_shell_surface_stress,
        NULL,
        NULL,
        shell_surf_shorts,
        shell_surf_longs,
        shell_surf_primals,
        QTY_SCLASS
    },

    {
        // Can calculate for G_TRI, G_QUAD
        { 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 1, 0, 0, 0 },
        TRUE,
        FALSE,
        compute_shell_strain,
        NULL,
        NULL,
        shell_strain_shorts,
        shell_strain_longs,
        shell_strain_primals,
        QTY_SCLASS
    },

    {
        // Can calculate for G_TRI, G_QUAD
        { 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 1, 0, 0, 0 },
        TRUE,
        FALSE,
        compute_shell_eff_strain,
        NULL,
        NULL,
        shell_eeff_shorts,
        shell_eeff_longs,
        shell_eeff_primals,
        QTY_SCLASS
    },

    {
        // Can calculate for G_TRI, G_QUAD
        { 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 1, 0, 0, 0 },
        TRUE,
        FALSE,
        compute_shell_stress,
        NULL,
        "Stress",
        shell_stress_shorts,
        shell_stress_longs,
        shell_stress_primals,
        QTY_SCLASS
    },

    /* Free Node results */
    {
        // Can calculate for G_MESH
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 1, 0, 0, 0 },
        FALSE,
        TRUE,
        compute_fnmass,
        NULL,
        NULL,
        fnmass_shorts,
        fnmass_longs,
        fnmass_primals,
        G_NODE
    },

    {
        // Can calculate for G_MESH
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 1, 0, 0, 0 },
        FALSE,
        TRUE,
        compute_anmass,
        NULL,
        NULL,
        anmass_shorts,
        anmass_longs,
        fnmass_primals,
        G_NODE
    },

    {
        // Can calculate for G_MESH
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 1, 0, 0, 0 },
        FALSE,
        TRUE,
        compute_fnmoment,
        NULL,
        NULL,
        fnmom_shorts,
        fnmom_longs,
        fnmass_primals,
        G_NODE
    },

    {
        // Can calculate for G_MESH
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0 },
        { 0, 1 },
        { 0, 0, 1, 0, 0, 1, 0, 0, 0 },
        FALSE,
        TRUE,
        compute_fnvol,
        NULL,
        NULL,
        fnvol_shorts,
        fnvol_longs,
        fnmass_primals,
        G_NODE
    },

    /* Array terminator */
    {
        { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
        { 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        TRUE,
        FALSE,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        QTY_SCLASS
    }
};


static Result empty_result =
{
    NULL,                       /* next in list */
    NULL,                       /* previous in list */
    { '\0' },                   /* Root */
    { '\0' },                   /* Name */
    { '\0' },                   /* Title */
    { '\0' },                   /* Original Name */
    NULL,                       /* Original Names */
    0,                          /* Quantity of supporting subrecords */
    NULL,                       /* Subrecord id array */
    NULL,                       /* Superclass array */
    NULL,                       /* Indirect result compuatation flags*/
    NULL,                       /* Computation function array */
    0,                          /* Mesh identifier */
    0,                          /* State record format identifier */
    NULL,                       /* Supporting primal results */
    { 0, 0, 0, 0, 0, 0, 0, 0, 0 }, /* Result origin flags */
    {                           /* Result specification */
        { '\0' },                   /* Name */
        INFINITESIMAL,              /* Strain type */
        MIDDLE,                     /* Reference surface */
        GLOBAL,                     /* Reference frame */
        0,                          /* Reference state */
        { 0, 0, 0, 0, 0, 0 }        /* Modifier use flags */
    },
    1,                          /* Logical - result spec is singly-valued */
    NULL,                       /* Ref to original derived or primal result */
    0                           /* Set to TRUE if the result must be recalaulcated */
};


/*****************************************************************
 * TAG( update_result )
 *
 * Update state record format-dependent data in current result.
 */
void
update_result( Analysis *analy, Result *p_result )
{
    int srec_id, mesh_id;
    int qty;
    Subrecord_result *p_sr;
    int i;
    List_head *srec_map;
    int *p_i;
    Subrecord *p_s;
    int st;

    /* Get srec format for current state. */
    st = analy->cur_state + 1;
    analy->db_query( analy->db_ident, QRY_SREC_FMT_ID,
                     (void *) &st, NULL, (void *) &srec_id );

    /* Update necessary only if state rec format has changed. */
    if ( srec_id != p_result->srec_id )
    {
        analy->db_query( analy->db_ident, QRY_SREC_MESH, (void *) &srec_id,
                         NULL, &mesh_id );
        p_result->mesh_id = mesh_id;

        /* Clean up shared allocations for previous srec format. */
        free( p_result->subrecs );
        free( p_result->superclasses );
        free( p_result->indirect_flags );
        free( p_result->result_funcs );

        if ( p_result->origin.is_derived )
        {
            /* Derived result-specific initializations. */

            free( p_result->primals );

            srec_map = ((Derived_result *) p_result->original_result)->srec_map;
            qty = srec_map[srec_id].qty;

            p_sr = (Subrecord_result *) srec_map[srec_id].list;

            /*
             * Not re-testing check functions here (they are originally called
             * by find_requested_result()) under assumption that a format
             * change won't affect anything a check function would evaluate.
             */

            if ( qty != 0 )
            {
                /* Arrays for results supported on one or more classes. */
                p_result->superclasses = NEW_N( int, qty,
                                                "Result sclass array" );
                p_result->subrecs = NEW_N( int, qty, "Result subrec array" );
                p_result->indirect_flags = NEW_N( Bool_type, qty,
                                                  "Result indirect flags" );

                /* Type incompatible with cast syntax of NEW_N.
                p_result->result_funcs = NEW_N( void (*)(), qty,
                                                     "Result func array" );
                */
                p_result->result_funcs = (void (**)()) calloc( qty,
                                         sizeof( void (**)() ) );
                p_result->primals = NEW_N( char **, qty,
                                           "Result primals array" );
            }

            for ( i = 0; i < qty; i++ )
            {
                p_result->subrecs[i] = p_sr[i].subrec_id;
                p_result->superclasses[i] = p_sr[i].superclass;
                p_result->indirect_flags[i] = p_sr[i].indirect;
                p_result->result_funcs[i] = p_sr[i].candidate->compute_func;
                p_result->primals[i] = p_sr[i].candidate->primals;
            }
        }
        else if ( p_result->origin.is_primal )
        {
            /* Primal result-specific initializations. */

            srec_map = ((Primal_result *) p_result->original_result)->srec_map;
            qty = srec_map[srec_id].qty;

            if ( qty != 0 )
            {
                /* Arrays for results supported on one or more classes. */
                p_result->superclasses = NEW_N( int, qty,
                                                "Result sclass array" );
                p_result->subrecs = NEW_N( int, qty, "Result subrec array" );
                p_result->indirect_flags = NEW_N( Bool_type, qty,
                                                  "Result indirect flags" );
                p_result->result_funcs = (void (**)()) calloc( qty,
                                         sizeof( void (**)() ) );
            }

            p_i = (int *) srec_map[srec_id].list;
            for ( i = 0; i < qty; i++ )
            {
                p_result->subrecs[i] = p_i[i];
                p_s = &(analy->srec_tree[srec_id].subrecs[p_i[i]].subrec);
                p_result->indirect_flags[i] = FALSE;
                analy->db_query( analy->db_ident, QRY_CLASS_SUPERCLASS, NULL,
                                 p_s->class_name,
                                 (void *) (p_result->superclasses + i) );
                p_result->result_funcs[i] = load_primal_result;
            }
        }

        if ( qty == 0 )
        {
            p_result->subrecs = NULL;
            p_result->superclasses = NULL;
            p_result->indirect_flags = NULL;
            p_result->result_funcs = NULL;
            p_result->primals = NULL;
        }

        p_result->qty = qty;
    }

    /* Always reset the modifiers. */
    p_result->modifiers = empty_result.modifiers;
}


/*****************************************************************
 * TAG( parse_result_spec )
 *
 * Parses a result specification string into its component parts.
 */
Bool_type
parse_result_spec( char *res_spec, char *root, int *p_qty_indices,
                   int index_array[], char *component )
{
    char spec[512];
    char *p_name;
    char *p_c_idx;
    char comp_str[G_MAX_NAME_LEN + 1];
    int indices[G_MAX_ARRAY_DIMS];
    int index_qty, i, last;
    char *delimiters = ", ]";

    strcpy( spec, res_spec );
    p_name = strtok( spec, "[" );

    /* Trim any trailing blanks. */
    last = strlen( p_name ) - 1;
    while ( p_name[last] == ' ' && last > -1 )
        last--;
    p_name[last + 1] = '\0';

    index_qty = 0;
    comp_str[0] = '\0';

    /* Parse to extract subset specification of aggregate svar. */
    p_c_idx = strtok( NULL, delimiters );
    while ( p_c_idx != NULL )
    {
        if ( is_numeric_token( p_c_idx ) )
        {
            indices[index_qty] = atoi( p_c_idx );
            index_qty++;
        }
        else /* Must be a vector comp_str short name. */
        {
            strcpy( comp_str, p_c_idx );

            /* Shouldn't be anything left after a comp_str name. */
            //if ( strtok( NULL, delimiters ) != NULL )
            //    return FALSE;
        }

        p_c_idx = strtok( NULL, delimiters );
    }

    /*
     * Reverse the order of the indices from Griz interface's row-major
     * order to internal (i.e., Mili's) column-major order.
     */
    *p_qty_indices = index_qty;
    for ( i = 0, last = index_qty - 1; i < index_qty; i++ )
        index_array[last - i] = indices[i];
    strcpy( component, comp_str );
    strcpy( root, p_name );

    return TRUE;
}



/*****************************************************************
 * TAG( find_result )
 *
 * Parses a result specification string and loads a Result struct.
 * Parameter "table" determines if result is searched for in
 * Derived result table, Primal result table, or both.
 */
int
find_result( Analysis *analy, Result_table_type table, Bool_type cur_srec_only,
             Result *p_result, char *name )
{
    int rval;
    Htable_entry *p_hte;
    char root[G_MAX_NAME_LEN + 1] = { '\0' };
    char component[G_MAX_NAME_LEN + 1] = { '\0' };
    char str[16];
    char col_major_name[128];
    Bool_type scalar = FALSE;
    Derived_result *p_dr;
    Primal_result *p_pr;
    State_variable *p_sv;
    int qty;
    int index_qty = 0;
    int indices[G_MAX_ARRAY_DIMS] = { 0 };
    int srec_id = 0;
    int mesh_id;
    Subrecord_result *p_sr;
    int *p_i;
    List_head *srec_map;
    Subrecord *p_s;
    int i, j;
    p_dr = NULL;
    p_pr = NULL;


    analy->result_active = FALSE; /* Used for error indicator which is only implemented
				       * for hexes currently. Set to false insure that
				       * that the EI message will not appear in the
				       * render window.
				       */

    rval = search_result_tables( analy, table, name,
                                 root, &index_qty, indices, component,
                                 &srec_id, &scalar, &p_dr, &p_pr,
                                 &srec_map);
    if ( !rval )
        return 0;

    /* Check for existence in current srec format. */
    if ( cur_srec_only && srec_map[srec_id].qty == 0 )
        return 0;
    else
        qty = srec_map[srec_id].qty;

    /* Clean up result struct. */
    cleanse_result( p_result );

    /* Initialize new result. */

    strcpy( p_result->original_name, name );
    strcpy( p_result->root, root );

    /*
     * Array and Vector-array names must be in column-major format
     * for database requests.
     */
    if ( component[0] != '\0' )
        strcpy( p_result->name, component );
    else
        strcpy( p_result->name, name );

    analy->db_query( analy->db_ident, QRY_SREC_MESH, (void *) &srec_id,
                     NULL, (void *) &mesh_id );
    p_result->mesh_id = mesh_id;
    p_result->srec_id = srec_id;
    p_result->qty = qty;
    p_result->single_valued = scalar;

    if ( qty > 0 )
    {
        /* Parallel arrays for descriptors across multiple classes. */
        p_result->superclasses = NEW_N( int, qty, "Result sclass array" );
        p_result->subrecs = NEW_N( int, qty, "Result subrec array" );
        p_result->indirect_flags = NEW_N( Bool_type, qty,
                                          "Result indirect flags" );
        p_result->result_funcs = (void (**)())
                                 calloc( qty, sizeof( void (**)() ) );
    }

    /*
     * Derived- and primal-specific initializations.
     *
     * Note that p_result->modifiers fields are set by the actual result
     * derivation routines that are invoked.  That way, if a shared result
     * has a modifier evaluated for a only a subset of all the classes that
     * support the result, the value of the modifier (such as "reference
     * frame") can be displayed only if an actual calculation occurred that
     * depended on modifier.
     */

    /*
     * Exception to above:  We need to init coord transform from the
     * current logical state (analy->do_tensor_transform) in order to
     * accommodate all the logic cases for detecting change and properly
     * updating derived results (see match_spec_with_analy()).
     */
    p_result->modifiers.use_flags.coord_transform =
        analy->do_tensor_transform;

    if ( p_dr != NULL )
    {
        /* Derived_result-specific initializations. */
        p_sr = (Subrecord_result *) srec_map[srec_id].list;
 
        /* Derived results need reference to supporting primals. */
        if ( qty > 0 )
            p_result->primals = NEW_N( char **, qty, "Result primals array" );

        for ( i = 0; i < qty; i++ )
        {
            p_result->subrecs[i] = p_sr[i].subrec_id;
            p_result->superclasses[i] = p_sr[i].superclass;
            p_result->indirect_flags[i] = p_sr[i].indirect;
            p_result->result_funcs[i] = p_sr[i].candidate->compute_func;
            p_result->primals[i] = p_sr[i].candidate->primals;
        }

        p_result->origin = p_dr->origin;
        p_result->original_result = (void *) p_dr;
        strcpy( p_result->title, p_sr[0].candidate->long_names[p_sr[0].index] );

        p_result->hide_in_menu = p_sr[0].candidate->hide_in_menu ;
    }
    else
    {
        /* Primal_result-specific initializations. */

        p_i = (int *) srec_map[srec_id].list;
        for ( i = 0; i < qty; i++ )
        {
            p_result->subrecs[i] = p_i[i];

            if(analy->srec_tree[srec_id].subrecs[p_i[i]].element_set)
            {
                setElementSet_integration_point(analy->srec_tree[srec_id].subrecs[p_i[i]].element_set,
                                                3, 1, indices[0]);
            }

            p_s = &(analy->srec_tree[srec_id].subrecs[p_i[i]].subrec);
            analy->db_query( analy->db_ident, QRY_CLASS_SUPERCLASS,
                             (void *) &mesh_id, p_s->class_name,
                             (void *) (p_result->superclasses + i) );
            p_result->indirect_flags[i] = FALSE;
            p_result->result_funcs[i] = ( p_pr->var->num_type == G_FLOAT8 )
                                        ? load_primal_result_double
                                        : load_primal_result;

            if ( p_pr->var->num_type == M_INT ||  p_pr->var->num_type == M_INT4 )
                p_result->result_funcs[i] = load_primal_result_int;

            if ( p_pr->var->num_type == M_INT8 )
                p_result->result_funcs[i] = load_primal_result_long;
        }

        p_result->origin = p_pr->origin;
        p_result->original_result = (void *) p_pr;

        if ( component[0] != '\0' )
        {
            /*
             * Append additional specifying text onto title.  Note that
             * this string is composed in Griz's UI standard row-major
             * format, unlike p_result->name, which is used to make
             * db requests.
             */
            if( p_pr->owning_vec_count > 0 ){
                for( j = 0; j < p_pr->owning_vec_count; j++ ){
                    if( strcmp(p_result->root, p_pr->owning_vector_result[j]->short_name) == 0 ){
                        strcpy( p_result->title, p_pr->owning_vector_result[j]->long_name);
                        strcat( p_result->title, "[" );
                        if ( index_qty > 0 )
                        {
                            sprintf( str, "%d", indices[index_qty - 1] );
                            strcat( p_result->title, str );
                        }
                        for ( i = index_qty - 2; i >= 0; i-- )
                        {
                            sprintf( str, ", %d", indices[i] );
                            strcat( p_result->title, str );
                        }

                        if ( index_qty > 0 )
                            sprintf( str, ", %s", p_pr->long_name );
                        else
                            strcpy( str, p_pr->long_name );
                        strcat( p_result->title, str );

                        strcat( p_result->title, "]" );
                        break;
                    }
                }
                if( j == p_pr->owning_vec_count ){
                    strcpy( p_result->title, p_pr->long_name );
                }
            }
        }
        else
        {
            strcpy( p_result->title, p_pr->long_name );
        }
    }

    return 1;
}


/*****************************************************************
 * TAG( search_result_tables )
 *
 * Search the result tables for a specified result.
 */
extern Bool_type
search_result_tables( Analysis *analy, Result_table_type table, char *name,
                      char *p_root, int *p_index_qty, int p_indices[],
                      char *p_component, int *p_srec_id, Bool_type *p_scalar,
                      Derived_result **pp_dr, Primal_result **pp_pr,
                      List_head **p_srec_map )
{
    int i;
    int qty_states, qty, index_qty;
    int srec_id, mesh_id;
    int st;
    Bool_type found, scalar;
    Htable_entry *p_hte;
    int rval;
    Subrecord_result *p_sr;
    char **comp_names;
    int comp_qty;
    char root[G_MAX_NAME_LEN + 1];
    char component[G_MAX_NAME_LEN + 1];
    int indices[G_MAX_ARRAY_DIMS];
    Derived_result *p_dr;
    Primal_result *p_pr;
    Bool_type (*check_func)( Analysis * );
    List_head *srec_map;

    /* No results if no results. */
    if ( analy->primal_results == NULL && analy->derived_results == NULL )
        return FALSE;

    /* Sanity check. */
    if ( name == NULL || name[0] == '\0' )
        return FALSE;

    /* No results if no states in db... */
    analy->db_query( analy->db_ident, QRY_QTY_STATES, NULL, NULL, (void *) &qty_states );
    if ( qty_states == 0 )
        return FALSE;

    /* Divide the specification into its component parts. */
    if ( !parse_result_spec( name, root, &index_qty, indices, component ) )
        return FALSE;

    if ( table == DERIVED && ( index_qty > 0 || component[0] != '\0' ) )
        /* There are no _derived_ vector or array results. */
        return FALSE;

    /* Get srec format and mesh ident for current state. */
    st = analy->cur_state + 1;
    analy->db_query( analy->db_ident, QRY_SREC_FMT_ID, (void *) &st, NULL, (void *) &srec_id );
    analy->db_query( analy->db_ident, QRY_SREC_MESH, (void *) &srec_id, NULL, (void *) &mesh_id );

    found = FALSE;
    scalar = TRUE;
    p_pr = NULL;
    p_dr = NULL;

    /*
     * Search tables indicated for specified result.  If all tables
     * are to be searched, look for a derived result first.
     */

    if ( table == PRIMAL || table == ALL )
    {
        if(component[0] == '\0')
            rval = htable_search( analy->primal_results, root, FIND_ENTRY, &p_hte );
        else
            rval = htable_search( analy->primal_results, component, FIND_ENTRY, &p_hte );

        if ( rval != OK && table == PRIMAL )
            return FALSE; /* Unable to match requested result. */
        else if( rval == OK ){
            /* Found Primal result matching name. */
            p_pr = (Primal_result *) p_hte->data;
            found = TRUE;

            /*
            * Determine if the specified request is ultimately scalar,
            * i.e., either the result is scalar or the result modified
            * by a subset specification results in a singly-valued quantity.
            */
            switch( p_pr->var->agg_type )
            {
            case VECTOR:
                if ( *component == '\0' )
                    scalar = FALSE;
                break;
            case ARRAY:
                if ( index_qty < p_pr->var->rank )
                    scalar = FALSE;
                break;
            case VEC_ARRAY:
                if ( *component == '\0' )
                    scalar = FALSE;
                else if ( index_qty < p_pr->var->rank )
                    scalar = FALSE;
                break;
            }

            /* Assign return parameters for primal results. */
            if ( p_srec_map != NULL )
                *p_srec_map = p_pr->srec_map;
            if ( pp_pr != NULL )
                *pp_pr = p_pr;
            if ( p_component != NULL )
                strcpy( p_component, component );
            if ( p_index_qty != NULL )
                *p_index_qty = index_qty;
            if ( p_indices != NULL )
                for ( i = 0; i < index_qty; i++ )
                    p_indices[i] = indices[i];
        }
    }

    if ( !found && ( table == DERIVED || table == ALL ) )
    {
        rval = htable_search( analy->derived_results, root, FIND_ENTRY, &p_hte );

        if ( rval != OK && table == DERIVED )
            return FALSE; /* Unable to match requested result. */
        else if ( rval == OK )
        {
            /* Found Derived result matching name. */
            p_dr = (Derived_result *) p_hte->data;

            /* Call check functions if extant to verify derivability. */
            srec_map = p_dr->srec_map;
            p_sr = (Subrecord_result *) srec_map[srec_id].list;
            qty = srec_map[srec_id].qty;

            for ( i = 0; i < qty; i++ )
            {
                check_func = p_sr[i].candidate->check_compute_func;
                if ( check_func != NULL && !check_func( analy ) )
                    return FALSE;
            }

            found = TRUE;

            /* Assign return parameters for derived results. */
            if ( pp_dr != NULL )
                *pp_dr = p_dr;
            if ( p_srec_map != NULL )
                *p_srec_map = srec_map;
        }
    }

    /* Assign remaining return parameters. */
    if ( p_root != NULL )
        strcpy( p_root, root );
    if ( p_srec_id != NULL )
        *p_srec_id = srec_id;
    if ( p_scalar != NULL )
        *p_scalar = scalar;

    return found;
}


/*****************************************************************
 * TAG( init_result )
 *
 * Overwrite a Result struct with empty fields.
 */
void
init_result( Result *p_r )
{
    *p_r = empty_result;
}


/*****************************************************************
 * TAG( cleanse_result )
 *
 * Clean up a Result struct.
 */
void
cleanse_result( Result *p_r )
{
    if ( p_r->next != NULL )
    {
        cleanse_result( p_r->next );
        p_r->next = NULL;
    }
        
    if ( p_r->superclasses != NULL )
        free( p_r->superclasses );

    if ( p_r->indirect_flags != NULL )
        free( p_r->indirect_flags );

    if ( p_r->result_funcs != NULL )
        free( p_r->result_funcs );

    if ( p_r->subrecs != NULL )
        free( p_r->subrecs );

    if ( p_r->primals != NULL )
        free( p_r->primals );

    init_result( p_r );
}


/*****************************************************************
 * TAG( delete_result )
 *
 * Destroy a Result struct.
 */
void
delete_result( Result **pp_r )
{
    /* Sanity check. */
    if ( *pp_r == NULL )
        return;

    cleanse_result( *pp_r );

    free( *pp_r );

    *pp_r = NULL;
}


/*****************************************************************
 * TAG( duplicate_result )
 *
 * Duplicate a Result struct, including allocations.
 */
Result *
duplicate_result( Analysis *analy, Result *p_res, Bool_type update_modifiers )
{
    int qty;
    Result *p_new;

    if ( p_res == NULL )
        return NULL;

    p_new = NEW( Result, "Duplicated result" );

    *p_new = *p_res;

    p_new->reference_count = 0;

    qty = p_res->qty;

    if ( qty > 0 )
    {
        p_new->subrecs = NEW_N( int, qty, "Dup Result subrecs" );
        memcpy( (void *) p_new->subrecs, (void *) p_res->subrecs,
                qty * sizeof( int ) );

        p_new->superclasses = NEW_N( int, qty, "Dup Result superclasses" );
        memcpy( (void *) p_new->superclasses, (void *) p_res->superclasses,
                qty * sizeof( int ) );

        p_new->indirect_flags = NEW_N( Bool_type, qty,
                                       "Dup Result indirect flags" );
        memcpy( (void *) p_new->indirect_flags, (void *) p_res->indirect_flags,
                qty * sizeof( Bool_type ) );

        p_new->result_funcs = (void (**)())
                              calloc( qty, sizeof( void (**)() ) );
        memcpy( (void *) p_new->result_funcs, (void *) p_res->result_funcs,
                qty * sizeof( (void *) NULL ) );

        if ( p_res->primals != NULL )
        {
            p_new->primals = NEW_N( char **, qty, "Dup Result primals" );
            memcpy( (void *) p_new->primals, (void *) p_res->primals,
                    qty * sizeof( (char *) NULL ) );
        }
    }

    if ( update_modifiers )
    {
        Result_spec *p_rs = &p_new->modifiers;

        if ( p_rs->use_flags.use_ref_surface )
            p_rs->ref_surf = analy->ref_surf;

        if ( p_rs->use_flags.use_ref_frame )
            p_rs->ref_frame = analy->ref_frame;

        if ( p_rs->use_flags.use_strain_variety )
            p_rs->strain_variety = analy->strain_variety;

        if ( p_rs->use_flags.use_ref_state )
            p_rs->ref_state = analy->reference_state;

        /*
         * We need to init coord transform from the current logical state
         * (analy->do_tensor_transform) in order to accommodate all the
         * logic cases for detecting change and properly updating derived
         * results (see match_spec_with_analy()).  Since the user's
         * dynamic preference determines whether or not this is set, we
         * don't copy it from an old Result but set it directly as we set
         * the _value_ of the other flags (we don't store the value of the
         * transformation matrix with the Result, just whether or not it's
         * used in result derivation).
         */
        p_rs->use_flags.coord_transform = analy->do_tensor_transform;
    }

    return p_new;
}


/*****************************************************************
 * TAG( result_has_class )
 *
 * Tests to see if a result exists on a particular class.
 */
Bool_type
result_has_class( Result *p_result, MO_class_data *p_mo_class, Analysis *analy )
{
    Derived_result *p_dr;
    Primal_result *p_pr = NULL;
    Subrecord_result *p_sr;
    int *p_i;
    int i, j;
    Subrec_obj *p_subr;
    int qty;

    if ( p_result == NULL )
        return FALSE;

    if ( p_result->origin.is_derived )
    {
        p_dr = (Derived_result *) p_result->original_result;

        for ( i = 0; i < analy->qty_srec_fmts; i++ )
        {
            if ( (qty = p_dr->srec_map[i].qty) > 0 )
            {
                p_sr = (Subrecord_result *) p_dr->srec_map[i].list;
                p_subr = analy->srec_tree[i].subrecs;

                for ( j = 0; j < qty; j++ )
                {
                    if ( !p_sr[j].indirect )
                    {
                        if ( p_subr[p_sr[j].subrec_id].p_object_class
                                == p_mo_class )
                            /* Found class match. */
                            return TRUE;
                    }
                    else
                    {
                        /*
                         * For "indirect" destination classes, just see if
                         * the test class superclass matches the subrecord
                         * candidate superclass.
                         */
                        if ( p_mo_class->superclass == p_sr[j].superclass )
                            return TRUE;
                    }
                }
            }
        }
    }
    else
    {
        p_pr = (Primal_result *) p_result->original_result;
        if(p_pr == NULL)
            return FALSE;
 
        for ( i = 0; i < analy->qty_srec_fmts; i++ )
        {
            if ( (qty = p_pr->srec_map[i].qty) > 0 )
            {
                p_i = (int *) p_pr->srec_map[i].list;
                p_subr = analy->srec_tree[i].subrecs;

                for ( j = 0; j < qty; j++ )
                    if ( p_subr[p_i[j]].p_object_class == p_mo_class ||
                            ( p_mo_class->superclass==M_HEX && is_particle_class( analy, p_subr[p_i[j]].p_object_class->superclass,
                                    p_subr[p_i[j]].p_object_class->short_name )) )

                        /* Found class match. */
                        return TRUE;
            }
        }
    }

    return FALSE;
}


/*****************************************************************
 * TAG( result_has_superclass )
 *
 * Tests to see if a result exists on a particular superclass.
 */
Bool_type
result_has_superclass( Result *p_result, int superclass, Analysis *analy )
{
    Derived_result *p_dr;
    Primal_result *p_pr = NULL;
    Subrecord_result *p_sr;
    int *p_i;
    int i, j, index;
    Bool_type found = FALSE;
    Bool_type is_derived = FALSE;
    Bool_type is_elemset = FALSE;
    Subrec_obj *p_subr;
    int qty;

    if ( p_result == NULL )
        return FALSE;


    /* with element sets we now need to deal with a result
     * that is a mixture of primal and derived results
     * so if the passed in superclass has a results function 
     * that is not load_primal_result then it needs
     * to be treated as derived even if the origin flag is set to primal
     */
    for(i = 0; i < p_result->qty; i++)
    {
        if(p_result->superclasses[i] == superclass)
        {
            if(p_result->result_funcs[i] != load_primal_result && p_result->result_funcs[i] != load_primal_result_double)
            {
                is_derived = TRUE;
                index = i;
                if(p_result->original_names != NULL && strstr(p_result->original_names[i], "es_") != NULL)
                {
                    is_elemset = TRUE;
                } 
                break;
            }
        }
    }

    if ( p_result->origin.is_derived || is_derived)
    {
        if( is_derived )
        {
           for(i = 0; i < p_result->qty; i++)
           {
               if(p_result->superclasses[i] == superclass)
               {
                   return TRUE;
               }
           }
           return FALSE; 
        }
        else
        {
            for(i = 0; i < p_result->qty; i++)
            {
                p_pr = (Primal_result *) analy->original_results[superclass];
                if(p_pr == NULL)
                {
                    return FALSE;
                }
            }
        }
    }
    else
    {
        p_pr = (Primal_result *) p_result->original_result;

        for ( i = 0; i < analy->qty_srec_fmts; i++ )
        {
            if ( (qty = p_pr->srec_map[i].qty) > 0 )
            {
                p_i = (int *) p_pr->srec_map[i].list;
                p_subr = analy->srec_tree[i].subrecs;

                for ( j = 0; j < qty; j++ )
                    if ( p_subr[p_i[j]].p_object_class->superclass
                            == superclass )
                        /* Found superclass match. */
                        return TRUE;
            }
        }
    }

    return FALSE;
}


/*****************************************************************
 * TAG( mod_required_plot_mode )
 *
 * Determine if current plot time series' are sensitive to a
 * transition in a result modifier.
 *
 * Note that TIME_DERIVATIVE is a "pseudo" modifier which will
 * never be passed in but occurs if strain variety is RATE and
 * the current result implements a simple history variable
 * rate calculation (initially only "eeff" implements this).
 *
 * Note that result->modifiers.use_flags.coord_transform is not
 * checked here.
 */
extern Bool_type
mod_required_plot_mode( Analysis *analy, Result_modifier_type modifier,
                        int old, int new )
{
    Result *p_res;
    Plot_obj *p_po;
    Time_series_obj *p_tso;

    for ( p_po = analy->current_plots; p_po != NULL; NEXT( p_po ) )
    {
        p_tso = p_po->ordinate;

        /* Check the time series' result. */
        p_res = p_tso->result;
        if ( mod_required( p_res, modifier, old, new ) )
            return TRUE;

        /*
         * Check operation plot operand results (technically, either
         * these will be non-NULL or the result above will be non-NULL).
         */
        if ( p_tso->operand1 != NULL )
        {
            p_res = p_tso->operand1->result;
            if ( mod_required( p_res, modifier, old, new ) )
                return TRUE;
        }

        if ( p_tso->operand2 != NULL )
        {
            p_res = p_tso->operand2->result;
            if ( mod_required( p_res, modifier, old, new ) )
                return TRUE;
        }

        /* Check abscissa change. */
        if ( p_po->abscissa->mo_class == NULL )
            continue;
        p_res = p_po->abscissa->result;
        if ( mod_required( p_res, modifier, old, new ) )
            return TRUE;
    }

    return FALSE;
}


/*****************************************************************
 * TAG( mod_required_mesh_mode )
 *
 * Determine if current result is sensitive to a transition in a
 * result modifier.
 *
 * This is just a stub to provide the same interface as
 * mod_required_plot_mode().
 */
extern Bool_type
mod_required_mesh_mode( Analysis *analy, Result_modifier_type modifier,
                        int old, int new )
{
    return mod_required( analy->cur_result, modifier, old, new );
}


/*****************************************************************
 * TAG( mod_required )
 *
 * Determine if current result is sensitive to a transition in a
 * result modifier.
 *
 * Note that TIME_DERIVATIVE is a "pseudo" modifier which will
 * never be passed in but occurs if strain variety is RATE and
 * the current result implements a simple history variable
 * rate calculation (initially only "eeff" implements this).
 *
 * Note that result->modifiers.use_flags.coord_transform is not
 * checked here.
 */
static Bool_type
mod_required( Result *p_res, Result_modifier_type modifier, int old, int new )
{
    /* Sanity checks. */
    if ( old == new
    || p_res == NULL )
        return FALSE;

    switch ( modifier )
    {
    case STRAIN_TYPE:
        if ( p_res->modifiers.use_flags.use_strain_variety )
            return TRUE;
        else if ( p_res->modifiers.use_flags.time_derivative
                  && old == RATE || new == RATE )
            return TRUE;
        break;
    case REFERENCE_SURFACE:
        if ( p_res->modifiers.use_flags.use_ref_surface )
            return TRUE;
        break;
    case REFERENCE_FRAME:
        if ( p_res->modifiers.use_flags.use_ref_frame )
            return TRUE;
        break;
    case REFERENCE_STATE:
        if ( p_res->modifiers.use_flags.use_ref_state )
            return TRUE;
        break;
    case COORD_TRANSFORM:
        /* Do nothing - placeholder. */
        break;
    }

    return FALSE;
}

/*****************************************************************
 * TAG( find_matching_subrec_index )
 *
 * Given a subrecord id and a primal result find the index of that subrecord
 * in the primal result.
 */
int find_matching_subrec_index(int subrec, Primal_result *primal_result){
    int i;
    int *list = (int*)primal_result->srec_map->list;
    for(i = 0; i < primal_result->srec_map->qty; i++)
    {
        if(list[i] == subrec)
            break;
    }
    return i;
}

/*****************************************************************
 * TAG( construct_result_query_string )
 *
 * Construct the query string for the result that we are looking up in the mili
 * database. Handle array and vector array naming formats.
 */
char * construct_result_query_string(Analysis* analy){
    int i, j, k, l;
    int rval, ipt_index;
    char target[15];
    Result* p_result;
    int index, subrec, srec;
    Subrec_obj* p_subrec;
    Bool_type found = FALSE;
    Bool_type is_old_shell_stress_result = FALSE;
    Primal_result* primal_result;
    Htable_entry *table_result;

    int owning_vec_idx, owning_owning_vec_idx;
    Primal_result *owning_vec, *owning_owning_vec;

    char * query_string;
    query_string = NEW_N(char, 64, "Result query string");

    p_result = analy->cur_result;
    index = analy->result_index;
    subrec = p_result->subrecs[index];
    srec = p_result->srec_id;
    p_subrec = analy->srec_tree[srec].subrecs + subrec;

    rval = htable_search(analy->primal_results, p_result->name, FIND_ENTRY, &table_result);

    if(rval == OK)
    {
        primal_result = (Primal_result*)table_result->data;
    }

    /* Get index of subrecord from primal_result that is being queried */
    i = find_matching_subrec_index(subrec, primal_result);

    target[0] = '\0';
    if( primal_result->owning_vec_count > 0 && primal_result->owning_vec_map[i] != -1 ){

        /* Get the owning vector for this result */
        owning_vec_idx = primal_result->owning_vec_map[i];
        owning_vec = primal_result->owning_vector_result[owning_vec_idx];

        /* Check if we are trying to show an old shell stress result. */
        if( analy->old_shell_stresses ){
            char* short_name = owning_vec->short_name;
            if(!strcmp(short_name, "stress_in") || !strcmp(short_name, "stress_mid") || !strcmp(short_name, "stress_out") ){
                is_old_shell_stress_result = TRUE;
            }
        }

        found = FALSE;

        /* Handle stress_in, stress_mid, stress_out results */
        if( is_old_shell_stress_result ){
            /* 2 Possibilities:
             *    1. asked for stress_[in|mid|out][component]
             *    2. asked for stress component and need to check reference surface
             */
            if(!strcmp(p_result->root, "stress_in") || !strcmp(p_result->root, "stress_mid") || !strcmp(p_result->root, "stress_out") ){
                strcpy(target, p_result->root);
            }
            else{
                switch(analy->ref_surf){
                    case INNER:
                        strcpy(target, "stress_in");
                        break;
                    case MIDDLE:
                        strcpy(target, "stress_mid");
                        break;
                    case OUTER:
                        strcpy(target, "stress_out");
                        break;
                }
                // Update reference surface flags for the result
                p_result->modifiers.use_flags.use_ref_surface = 1;
                p_result->modifiers.ref_surf = analy->ref_surf;
                p_result->modifiers.use_flags.use_ref_frame = 1;
                p_result->modifiers.ref_frame = analy->ref_frame;
            }

            /* There is a possibility that stress_in, stress_mid, and stress_out are all
             * in the same subrecord which invalidates the owning_vec_map. Need to lookup
             * correct owning vector */
            found = FALSE;
            for( j = 0; j < primal_result->owning_vec_count; j++ ){
                owning_vec = primal_result->owning_vector_result[j];
                if( target[0] == '\0' || !strcmp(target, owning_vec->short_name) ){
                    k = find_matching_subrec_index( subrec, owning_vec );
                    if ( k != owning_vec->qty_subrecs ){
                        found = TRUE;
                        break;
                    }
                }
            }

            if( found )
            {
                strcpy(query_string, owning_vec->original_names_per_subrec[k]);
                sprintf(query_string, "%s[%s]" , query_string, p_result->name);
            }
            else
            {
                sprintf(query_string, "%s", p_result->name);
            }
        }
        else
        {
            /* Get subrecord in owning vector that matches subrecord of component */
            j = find_matching_subrec_index(subrec, owning_vec);
            if(j != owning_vec->qty_subrecs){
                found = TRUE;
            }

            if( found ){
                /* Check for vector array containing the owning vector of the current result. */
                found = FALSE;
                if( owning_vec->in_vector_array && owning_vec->owning_vec_map[j] != -1 ){
                    owning_owning_vec_idx = owning_vec->owning_vec_map[j];
                    owning_owning_vec = owning_vec->owning_vector_result[owning_owning_vec_idx];
                    k = find_matching_subrec_index( subrec, owning_owning_vec );
                    if ( k != owning_owning_vec->qty_subrecs ){
                        found = TRUE;
                    }
                    
                    if( found )
                    {
                        strcpy(query_string, owning_owning_vec->original_names_per_subrec[k]);
                        if(p_subrec->element_set->tempIndex < 0)
                            ipt_index = p_subrec->element_set->current_index+1;
                        else
                            ipt_index = p_subrec->element_set->tempIndex+1;
                        sprintf(query_string,"%s[%d,%s[%s]]" , query_string, ipt_index,
                                owning_vec->original_names_per_subrec[j], primal_result->short_name);
                    }
                    else
                    {
                        strcpy(query_string, owning_vec->original_names_per_subrec[j]);
                        sprintf(query_string, "%s[%s]" , query_string, p_result->name);
                    }
                }
                else if(p_subrec->element_set){
                    strcpy(query_string, owning_vec->original_names_per_subrec[j]);
                    if(p_subrec->element_set->tempIndex < 0)
                        ipt_index = p_subrec->element_set->current_index+1;
                    else
                        ipt_index = p_subrec->element_set->tempIndex+1;
                    sprintf(query_string,"%s[%d,%s]" , query_string, ipt_index, p_result->name);
                }
                /* If this is not an element set, we need to add in result name */
                else
                {
                    strcpy(query_string, owning_vec->original_names_per_subrec[j]);
                    sprintf(query_string, "%s[%s]" , query_string, p_result->name);
                }
            }
            else{
                sprintf(query_string, "%s", p_result->name);
            }
        }
    }
    else if(primal_result->var->agg_type == ARRAY)
    {
        strcpy(query_string, analy->cur_result->name);
    }
    else
    {
        strcpy(query_string, primal_result->original_names_per_subrec[i]);
    }

    return query_string;
}


/*****************************************************************
 * TAG( stress_strain_rloc_warning_message )
 *
 * If the result is a primal stress/strain component and the reference frame is set to LOCAL,
 * then output a warning message saying that Global to local transformation is not implemented for
 * primals currently. Once that has been implemented this error message can be removed.
 */
void
stress_strain_rloc_warning_message( Analysis * analy )
{
    static Bool_type warn = TRUE;
    Bool_type is_primal_stress_strain = FALSE;
    Result * p_result;

    /* If message already shown, just bail */
    if( !warn )
        return;

    p_result = analy->cur_result;
    if( p_result->origin.is_primal )
    {
        if( strncmp( p_result->name, "e", 1 ) == 0 || strncmp( p_result->name, "s", 1 ) == 0 )
        {
            if( strncmp( p_result->name+1, "x", 1 ) == 0
                || strncmp( p_result->name+1, "y", 1 ) == 0
                || strncmp( p_result->name+1, "z", 1 ) == 0
                || strncmp( p_result->name+1, "xy", 2 ) == 0
                || strncmp( p_result->name+1, "yz", 2 ) == 0
                || strncmp( p_result->name+1, "zx", 2 ) == 0 )
            {
                is_primal_stress_strain = TRUE;
            }
        }
        if( strncmp( p_result->name, "stress", 6 ) == 0 || strncmp( p_result->name, "strain", 6 ) == 0 )
        {
            is_primal_stress_strain = TRUE;
        }
    }

    if( warn && is_primal_stress_strain && analy->ref_frame == LOCAL )
    {
        popup_dialog(WARNING_POPUP, "%s%s%s%s",
                     "Reference frame set to LOCAL, but querying primal stress/strain.\n",
                     "Global to local transformation not currently available for primal quantities.\n",
                     "You will need to run the command \"sw derived_results\" to see the transformation.\n",
                     "NOTE: This warning will only display ONCE");
        warn = FALSE;
    }
}


/*****************************************************************
 * TAG( load_primal_result )
 *
 * Load a scalar result from a subrecord without performing any
 * derivation.  This function can be used to provide "derived"
 * result entries from what is actually a _scalar_ primal result,
 * but as configured will load a non-scalar primal into
 * analy->tmp_result[0] (which could overflow).
 */
void
load_primal_result( Analysis *analy, float *resultArr, Bool_type interpolate )
{
    float *resultElem;
    int i, rval;
    float *result_buf;
    Result *p_result;
    Result test_result;
    int subrec, srec;
    int obj_qty, len;
    int es_qty;
    int index, j, k, l;
    int ipt_index;
    int *object_ids;
    Subrec_obj *p_subrec;
    char *primals[2];

    p_result = analy->cur_result;
    index = analy->result_index;
    subrec = p_result->subrecs[index];
    srec = p_result->srec_id;
    p_subrec = analy->srec_tree[srec].subrecs + subrec;
    object_ids = p_subrec->object_ids;
    obj_qty = p_subrec->subrec.qty_objects;
    float * activity;

    /* Get the name to search for in the Mili database */
    primals[0] = construct_result_query_string(analy);
    primals[1] = NULL;

    stress_strain_rloc_warning_message( analy );

    analy->result_active = FALSE; /* Used for error indicator which is only implemented
			                       * for hexes currently. Set to false insure that
				                   * that the EI message will not appear in the
				                   * render window. */

    if(analy->state_p->sand_present && p_subrec->p_object_class->elem_class_index >= 0)
        activity = analy->state_p->elem_class_sand[p_subrec->p_object_class->elem_class_index];
    else
        activity = NULL;

    if ( !analy->particle_nodes_enabled
            && is_particle_class( analy, p_subrec->p_object_class->superclass, p_subrec->p_object_class->short_name ) )
        return;

    if((p_result->superclasses[index] == G_QUAD || p_result->superclasses[index] == G_TRI) && analy->ref_frame == LOCAL)
    {
        /* we have a shell result in the LOCCAL coordinate system. Now check for a stress result */
        len = strlen(primals[0]);
        if(len == 2)
        {
            if(!strcmp(primals[0], "sx") || !strcmp(primals[0], "sy") || !strcmp(primals[0], "sz"))
            {
                load_stress_local_coord(analy, NODAL_RESULT_BUFFER(analy), interpolate);
                return;
            }
        } else if(len == 3)
        {
            if(!strcmp(primals[0], "sxy") || !strcmp(primals[0], "szy") || !strcmp(primals[0], "szx"))
            {
                load_stress_local_coord(analy, NODAL_RESULT_BUFFER(analy), interpolate);
                return;
            }
        } else if(len > 6)
        { 
                if(!strncmp(primals[0], "stress", 6) )
                {
                    load_stress_local_coord(analy, NODAL_RESULT_BUFFER(analy), interpolate);
                    return;
                }
        }
        
    }    

    if ( object_ids )
    {
        /*
         * Assign result_buf to a temp array for db read below, and assign
         * resultElem to put the sorted data into the correct array by
         * superclass.  This will leave non-interpolated data in the
         * correct location to support such needs as time-history extraction.
         */
        result_buf = analy->tmp_result[1];

        if ( p_result->origin.is_node_result )
            resultElem = resultArr;  /* hack - resultElem is misleading. */
        else
            resultElem = p_subrec->p_object_class->data_buffer;
    }
    else if ( p_result->origin.is_node_result )
        result_buf = resultArr;
    else if ( p_result->single_valued )
        result_buf = p_subrec->p_object_class->data_buffer;
    else
        result_buf = analy->tmp_result[0];

    /* Read the database. */
    analy->db_get_results( analy->db_ident, analy->cur_state + 1, subrec, 1,
                           primals, (void *) result_buf );

    /* Re-order data if necessary. */
    if ( object_ids )
    {
        /* If we ge a particle object and we do not have particles on, then set value to zero */
        for ( i = 0; i < obj_qty; i++ )
            resultElem[object_ids[i]] = result_buf[i];

        /*
         * Re-assign result_buf with data now sorted in class-sized arrays
         * to support interpolation and/or min/max extraction below.
         */
        result_buf = resultElem;
    }

    /*
     * Interpolate element results back to the nodes and/or
     * extract object data min/max.
     */
    if ( p_result->origin.is_elem_result )
    {
        if ( interpolate )
        {
            switch ( p_result->superclasses[index] )
            {
            case G_TRUSS:
                truss_to_nodal( result_buf, resultArr,
                                p_subrec->p_object_class, obj_qty,
                                object_ids, analy );
                break;
            case G_BEAM:
                beam_to_nodal( result_buf, resultArr,
                               p_subrec->p_object_class, obj_qty,
                               object_ids, analy );
                break;
            case G_TRI:
                tri_to_nodal( result_buf, resultArr,
                              p_subrec->p_object_class, obj_qty,
                              object_ids, analy, TRUE );
                break;
            case G_QUAD:
                if (is_primal_quad_strain_result( p_result->name ))
                {
                    rotate_quad_result( analy, p_result->name, -1, result_buf );
                }
                quad_to_nodal( result_buf, resultArr,
                               p_subrec->p_object_class, obj_qty,
                               object_ids, analy, TRUE );
                break;
            case G_TET:
                tet_to_nodal( result_buf, resultArr,
                              p_subrec->p_object_class, obj_qty,
                              object_ids, analy );
                break;
            case G_HEX:
                hex_to_nodal( result_buf, resultArr,
                              p_subrec->p_object_class, obj_qty,
                              object_ids, analy );
                break;
            case G_PARTICLE:
                particle_to_nodal( result_buf, resultArr,
                                   p_subrec->p_object_class, obj_qty,
                                   object_ids, analy );
                break;
            case G_SURFACE:
                surf_to_nodal( result_buf, resultArr,
                               p_subrec->p_object_class, obj_qty,
                               object_ids, analy, TRUE );
                break;
            }
        }
        else
        {
            if(  p_result->superclasses[index] != G_SURFACE )
                elem_get_minmax( result_buf, 0, analy );

            if ( p_result->superclasses[index] == G_QUAD )
            {
                if (is_primal_quad_strain_result( p_result->name ))
                {
                    i=0;
                }
            }
        }
    }
    else if ( !p_result->origin.is_node_result )
    {
        switch ( p_result->superclasses[index] )
        {
        case G_UNIT:
            unit_get_minmax( result_buf, analy );
            break;
        case G_MAT:
            mat_get_minmax( result_buf, analy );
            break;
        case G_MESH:
            mesh_get_minmax( result_buf, analy );
            break;
        }
    }

    // Free result query string
    free(primals[0]);
}

/*****************************************************************
 * TAG( load_stress_local_coord )
 *
 * Load a scalar stress result from a subrecord without performing any
 * derivation in the local coordinate system. This function is called from
 * load_primal_result(...) where the current result involves the shell class
 * a stress result and when analy->ref_frame == LOCAL 
 */

void
load_stress_local_coord( Analysis *analy, float *resultArr, Bool_type interpolate )
{
    float * resultElem;
    float * result_buf;
    int i, j, subrec, srec, obj_qty, rval;
    int qty = 0;
    int es_qty;
    int index, idx, elem_idx;
    int * object_ids;
    Result *p_result;
    Subrec_obj *p_subrec;
    char *primals[2];
    char *primal_list[7];
    char primal_spec[32];
    char name[32];
    char *p;
    Bool_type map_timehist_coords = FALSE;
    Bool_type found = FALSE;
    GVec3D2P *new_nodes = NULL;
    float localMat[3][3];
    float (* sigma)[6];
    MO_class_data *p_mo_class;
   
    p_result = analy->cur_result;
    index = analy->result_index;
    subrec = p_result->subrecs[index];
    srec = p_result->srec_id;
    p_subrec = analy->srec_tree[srec].subrecs + subrec;
    object_ids = p_subrec->object_ids;
    obj_qty = p_subrec->subrec.qty_objects;
    Hash_table *p_sv_ht;
    Htable_entry *p_hte;
    State_variable *p_sv;
    p_sv_ht = analy->st_var_table;

    /* initialize char pointers to NULL */
    for(i = 0; i < 7; i++)
    {
        primal_list[i] = NULL;
        if(i < 2)
        {
            primals[i] = NULL;
        }
    } 

    if(p_result->qty > 1 && p_result->original_names != NULL)
    {
        for(i = 0; i < p_subrec->subrec.qty_svars; i++)
        {
            if(strstr(p_result->original_names[index], p_subrec->subrec.svar_names[i]) != NULL)
            {
                strcpy(primal_spec, p_result->original_names[index]);
                break;
            }
        }
    }
    else
    {
        strcpy(primal_spec, p_result->name);
    } 

    p_mo_class = p_subrec->p_object_class;

    /* we know that we are in the local coordinate system or this function would not be called.  We also know that
 *     we are dealing with shell stresses.  Just need to get the primal name like "sx", "szx" etc.  
 *     or one of sx sy sz sxy syz szx */
   if(strlen(primal_spec) <= 3)
   {
       idx = (int) primal_spec[1] - (int) 'x' + ((primal_spec[2] ) ? 3: 0);
   } else
   {
       for(p = &primal_spec[0]; *p != '['; p++);
       p++;
       i = 0;
       while(*p != ']' && *p != ',')
       {
          name[i] = *p;
          p++;
          i++; 
       }
       name[i] = '\0';
       idx = (int) name[1] - (int) 'x' + ((name[2] ) ? 3: 0);
   }

    for(i = 0; i < 6; i++)
    {
        primal_list[i] = malloc(32*sizeof(char));
    }
     
    if(p_result->original_names != NULL)
    {
        if(strstr(p_result->original_names[index], "stress_in") != NULL)
        {
            strcpy(name, "stress_in");
        } else if(strstr(p_result->original_names[index], "stress_mid") != NULL)
        {
            strcpy(name, "stress_mid");
        } else if(strstr(p_result->original_names[index], "stress_out") != NULL)
        {
            strcpy(name, "stress_out");
        } else
        {
            strcpy(name, "stress");
        }
    } else
    {
        if(strstr(p_result->original_name, "stress_in") != NULL)
        {
            strcpy(name, "stress_in");
        } else if(strstr(p_result->original_name, "stress_mid") != NULL)
        {
            strcpy(name, "stress_mid");
        } else if(strstr(p_result->original_name, "stress_out") != NULL)
        {
            strcpy(name, "stress_out");
        } else
        {
            strcpy(name, "stress");
        }
    }
        
    found = FALSE;
    primals[0] = malloc(strlen(name) + 1);
    strcpy(primals[0], name);
    primals[1] = NULL;

   primal_list[6] = NULL;
 
   resultElem = p_subrec->p_object_class->data_buffer;
   result_buf = analy->tmp_result[0];

   if(found)
   {
       analy->db_get_results(analy->db_ident, analy->cur_state + 1, subrec, 6, primal_list, (void *) result_buf);
   } else
   {
       analy->db_get_results(analy->db_ident, analy->cur_state + 1, subrec, 1, primals, (void *) result_buf);
   }
   
   sigma = (float(*)[6]) result_buf;

   if(object_ids)
   {
           if(p_mo_class->superclass == G_QUAD)
           {   for(i = 0; i < obj_qty; i++)
               {
                   elem_idx = object_ids[i];
                   global_to_local_mtx(analy, p_mo_class, elem_idx, map_timehist_coords, new_nodes, localMat);
                   transform_tensors_1p(1, sigma + i, localMat);
                   resultElem[elem_idx] = sigma[i][idx];
               }
           } else if(p_mo_class->superclass == G_TRI)
           {   for(i = 0; i < obj_qty; i++)
               {
                   elem_idx = object_ids[i];
                   global_to_local_tri_mtx(analy, p_mo_class, elem_idx, map_timehist_coords, new_nodes, localMat);
                   transform_tensors_1p(1, sigma + i, localMat);
                   resultElem[elem_idx] = sigma[i][idx];
               }
           }
   } else
   {
       if(p_mo_class->superclass == G_QUAD)
       {
           for(i = 0; i < obj_qty; i++)
           {
               global_to_local_mtx(analy, p_mo_class, i, map_timehist_coords, new_nodes, localMat);
               transform_tensors_1p(1, sigma + i, localMat);
               resultElem[i] = sigma[i][idx];
           }
       } else if(p_mo_class->superclass == G_TRI)
       {
           for(i = 0; i < obj_qty; i++)
           {
               global_to_local_tri_mtx(analy, p_mo_class, i, map_timehist_coords, new_nodes, localMat);
               transform_tensors_1p(1, sigma + i, localMat);
               resultElem[i] = sigma[i][idx];
           }
       }
   } 
   p_result->modifiers.use_flags.use_ref_frame = 1;
   p_result->modifiers.ref_frame = analy->ref_frame;
   p_result->modifiers.use_flags.use_ref_surface = 1;
   p_result->modifiers.ref_surf = analy->ref_surf;

   if(interpolate)
   {
       if(p_mo_class->superclass == G_QUAD)
       {
           quad_to_nodal( resultElem, resultArr, p_mo_class, obj_qty, object_ids, analy, TRUE);
       } else if(p_mo_class->superclass == G_TRI)
       {
           tri_to_nodal( resultElem, resultArr, p_mo_class, obj_qty, object_ids, analy, TRUE );
       }
   }

    if(primal_list[0] != NULL)
    {
        for(i = 0; i < 6; i++)
        {
            free(primal_list[i]);    
        }
    }
    if(primals[0] != NULL)
    {
        free(primals[0]);
    }
  
}

/*****************************************************************
 * TAG( load_primal_result_double )
 *
 * Load a scalar double precision result from a subrecord,
 * converting it to single precision.
 */
void
load_primal_result_double( Analysis *analy, float *resultArr,
                           Bool_type interpolate )
{
    float *resultElem;
    int i, j, k, l, rval;
    Result *p_result;
    int subrec, srec;
    int obj_qty;
    int index;
    int *object_ids;
    Subrec_obj *p_subrec;
    char *primals[2];
    char primal_spec[32];
    char target[15];
    double *dbuffer;
    Primal_result* primal_result;
    Htable_entry *table_result;
    int ipt_index;
    Bool_type found = FALSE;
    Bool_type is_old_shell_stress_result = FALSE;


    analy->result_active = FALSE; /* Used for error indicator which is only implemented
				   * for hexes currently. Set to false insure that
				   * that the EI message will not appear in the
				   * render window.
				   */

    p_result = analy->cur_result;
    index = analy->result_index;
    subrec = p_result->subrecs[index];
    srec = p_result->srec_id;
    p_subrec = analy->srec_tree[srec].subrecs + subrec;
    object_ids = p_subrec->object_ids;
    obj_qty = p_subrec->subrec.qty_objects;

    dbuffer = NEW_N( double, obj_qty, "Double precision input buffer" );

    /* Get the name to search for in the Mili database */
    primals[0] = construct_result_query_string(analy);
    primals[1] = NULL;

    /* Read the database. */
    analy->db_get_results( analy->db_ident, analy->cur_state + 1, subrec, 1,
                           primals, (void *) dbuffer );

    if ( p_result->origin.is_node_result )
        resultElem = resultArr;  /* hack - resultElem is misleading. */
    else
        resultElem = p_subrec->p_object_class->data_buffer;

    /* Re-order data if necessary. */
    if ( object_ids )
    {
        for ( i = 0; i < obj_qty; i++ )
            resultElem[object_ids[i]] = (float) dbuffer[i];
    }
    else
    {
        for ( i = 0; i < obj_qty; i++ )
            resultElem[i] = (float) dbuffer[i];
    }

    free( dbuffer );

    /*
     * Interpolate element results back to the nodes and/or
     * extract object data min/max.
     */
    if ( p_result->origin.is_elem_result )
    {
        if ( interpolate )
        {
            switch ( p_result->superclasses[index] )
            {
            case G_TRUSS:
                truss_to_nodal( resultElem, resultArr,
                                p_subrec->p_object_class, obj_qty,
                                object_ids, analy );
                break;
            case G_BEAM:
                beam_to_nodal( resultElem, resultArr,
                               p_subrec->p_object_class, obj_qty,
                               object_ids, analy );
                break;
            case G_TRI:
                tri_to_nodal( resultElem, resultArr,
                              p_subrec->p_object_class, obj_qty,
                              object_ids, analy, TRUE );
                break;
            case G_QUAD:
                quad_to_nodal( resultElem, resultArr,
                               p_subrec->p_object_class, obj_qty,
                               object_ids, analy, TRUE );
                break;
            case G_TET:
                tet_to_nodal( resultElem, resultArr,
                              p_subrec->p_object_class, obj_qty,
                              object_ids, analy );
                break;
            case G_HEX:
                hex_to_nodal( resultElem, resultArr,
                              p_subrec->p_object_class, obj_qty,
                              object_ids, analy );
                break;
            case G_PARTICLE:
                particle_to_nodal( resultElem, resultArr,
                                   p_subrec->p_object_class, obj_qty,
                                   object_ids, analy );
                break;
            case G_SURFACE:
                surf_to_nodal( resultElem, resultArr,
                               p_subrec->p_object_class, obj_qty,
                               object_ids, analy, TRUE );
                break;
            }
        }
        else
        {
            elem_get_minmax( resultElem, 0 , analy );
        }
    }
    else if ( !p_result->origin.is_node_result )
    {
        switch ( p_result->superclasses[index] )
        {
        case G_UNIT:
            unit_get_minmax( resultElem, analy );
            break;
        case G_MAT:
            mat_get_minmax( resultElem, analy );
            break;
        case G_MESH:
            mesh_get_minmax( resultElem, analy );
            break;
        }
    }

    /* Free result query string */
    free(primals[0]);
}


/*****************************************************************
 * TAG( load_primal_result_int )
 *
 * Load a scalar int result from a subrecord,
 * converting it to single precision.
 */
void
load_primal_result_int( Analysis *analy, float *resultArr,
                        Bool_type interpolate )
{
    float *resultElem;
    int i, j, k, l, rval;
    Result *p_result;
    int subrec, srec;
    int obj_qty;
    int index;
    int *object_ids;
    Subrec_obj *p_subrec;
    char *primals[2];
    char primal_spec[32];
    char target[15];
    int *ibuffer;
    Primal_result* primal_result;
    Htable_entry *table_result;
    int ipt_index;
    Bool_type found = FALSE;
    Bool_type is_old_shell_stress_result = FALSE;

    analy->result_active = FALSE; /* Used for error indicator which is only implemented
				   * for hexes currently. Set to false insure that
				   * that the EI message will not appear in the
				   * render window.
				   */

    p_result = analy->cur_result;
    index = analy->result_index;
    subrec = p_result->subrecs[index];
    srec = p_result->srec_id;
    p_subrec = analy->srec_tree[srec].subrecs + subrec;
    object_ids = p_subrec->object_ids;
    obj_qty = p_subrec->subrec.qty_objects;

    ibuffer = NEW_N( int, obj_qty, "integer single precision input buffer" );

    /* Get the name to search for in the Mili database */
    primals[0] = construct_result_query_string(analy);
    primals[1] = NULL;

    /* Read the database. */
    analy->db_get_results( analy->db_ident, analy->cur_state + 1, subrec, 1,
                           primals, (void *) ibuffer );

    if ( p_result->origin.is_node_result )
        resultElem = resultArr;  /* hack - resultElem is misleading. */
    else
        resultElem = p_subrec->p_object_class->data_buffer;

    /* Re-order data if necessary. */
    if ( object_ids )
    {
        for ( i = 0; i < obj_qty; i++ )
            resultElem[object_ids[i]] = (float) ibuffer[i]/1.0;
    }
    else
    {
        for ( i = 0; i < obj_qty; i++ )
            resultElem[i] = (float) ibuffer[i]/1.0;
    }

    free( ibuffer );

    /*
     * Interpolate element results back to the nodes and/or
     * extract object data min/max.
     */
    if ( p_result->origin.is_elem_result )
    {
        if ( interpolate )
        {
            switch ( p_result->superclasses[index] )
            {
            case G_TRUSS:
                truss_to_nodal( resultElem, resultArr,
                                p_subrec->p_object_class, obj_qty,
                                object_ids, analy );
                break;
            case G_BEAM:
                beam_to_nodal( resultElem, resultArr,
                               p_subrec->p_object_class, obj_qty,
                               object_ids, analy );
                break;
            case G_TRI:
                tri_to_nodal( resultElem, resultArr,
                              p_subrec->p_object_class, obj_qty,
                              object_ids, analy, TRUE );
                break;
            case G_QUAD:
                quad_to_nodal( resultElem, resultArr,
                               p_subrec->p_object_class, obj_qty,
                               object_ids, analy, TRUE );
                break;
            case G_TET:
                tet_to_nodal( resultElem, resultArr,
                              p_subrec->p_object_class, obj_qty,
                              object_ids, analy );
                break;
            case G_HEX:
                hex_to_nodal( resultElem, resultArr,
                              p_subrec->p_object_class, obj_qty,
                              object_ids, analy );
                break;
            case G_PARTICLE:
                particle_to_nodal( resultElem, resultArr,
                                   p_subrec->p_object_class, obj_qty,
                                   object_ids, analy );
                break;
            case G_SURFACE:
                surf_to_nodal( resultElem, resultArr,
                               p_subrec->p_object_class, obj_qty,
                               object_ids, analy, TRUE );
                break;
            }
        }
        else
        {
            elem_get_minmax( resultElem, 0, analy );
        }
    }
    else if ( !p_result->origin.is_node_result )
    {
        switch ( p_result->superclasses[index] )
        {
        case G_UNIT:
            unit_get_minmax( resultElem, analy );
            break;
        case G_MAT:
            mat_get_minmax( resultElem, analy );
            break;
        case G_MESH:
            mesh_get_minmax( resultElem, analy );
            break;
        }
    }

    /* Free result query string */
    free(primals[0]);
}


/*****************************************************************
 * TAG( load_primal_result_long )
 *
 * Load a scalar long result from a subrecord,
 * converting it to single precision.
 */
void
load_primal_result_long( Analysis *analy, float *resultArr,
                         Bool_type interpolate )
{
    float *resultElem;
    int i, j, k, l, rval;
    Result *p_result;
    int subrec, srec;
    int obj_qty;
    int index;
    int *object_ids;
    Subrec_obj *p_subrec;
    char *primals[2];
    char primal_spec[32];
    char target[15];
    long *ibuffer;
    Primal_result* primal_result;
    Htable_entry *table_result;
    int ipt_index;
    Bool_type found = FALSE;
    Bool_type is_old_shell_stress_result = FALSE;

    analy->result_active = FALSE; /* Used for error indicator which is only implemented
				   * for hexes currently. Set to false insure that
				   * that the EI message will not appear in the
				   * render window.
				   */
    p_result = analy->cur_result;
    index = analy->result_index;
    subrec = p_result->subrecs[index];
    srec = p_result->srec_id;
    p_subrec = analy->srec_tree[srec].subrecs + subrec;
    object_ids = p_subrec->object_ids;
    obj_qty = p_subrec->subrec.qty_objects;

    ibuffer = NEW_N( long, obj_qty, "integer single precision input buffer" );

    /* Get the name to search for in the Mili database */
    primals[0] = construct_result_query_string(analy);
    primals[1] = NULL;

    /* Read the database. */
    analy->db_get_results( analy->db_ident, analy->cur_state + 1, subrec, 1,
                           primals, (void *) ibuffer );

    if ( p_result->origin.is_node_result )
        resultElem = resultArr;  /* hack - resultElem is misleading. */
    else
        resultElem = p_subrec->p_object_class->data_buffer;

    /* Re-order data if necessary. */
    if ( object_ids )
    {
        for ( i = 0; i < obj_qty; i++ )
            resultElem[object_ids[i]] = (float) ibuffer[i];
    }
    else
    {
        for ( i = 0; i < obj_qty; i++ )
            resultElem[i] = (float) ibuffer[i];
    }

    free( ibuffer );

    /*
     * Interpolate element results back to the nodes and/or
     * extract object data min/max.
     */
    if ( p_result->origin.is_elem_result )
    {
        if ( interpolate )
        {
            switch ( p_result->superclasses[index] )
            {
            case G_TRUSS:
                truss_to_nodal( resultElem, resultArr,
                                p_subrec->p_object_class, obj_qty,
                                object_ids, analy );
                break;
            case G_BEAM:
                beam_to_nodal( resultElem, resultArr,
                               p_subrec->p_object_class, obj_qty,
                               object_ids, analy );
                break;
            case G_TRI:
                tri_to_nodal( resultElem, resultArr,
                              p_subrec->p_object_class, obj_qty,
                              object_ids, analy, TRUE );
                break;
            case G_QUAD:
                quad_to_nodal( resultElem, resultArr,
                               p_subrec->p_object_class, obj_qty,
                               object_ids, analy, TRUE );
                break;
            case G_TET:
                tet_to_nodal( resultElem, resultArr,
                              p_subrec->p_object_class, obj_qty,
                              object_ids, analy );
                break;
            case G_HEX:
                hex_to_nodal( resultElem, resultArr,
                              p_subrec->p_object_class, obj_qty,
                              object_ids, analy );
                break;
            case G_PARTICLE:
                particle_to_nodal( resultElem, resultArr,
                                   p_subrec->p_object_class, obj_qty,
                                   object_ids, analy );
                break;
            case G_SURFACE:
                surf_to_nodal( resultElem, resultArr,
                               p_subrec->p_object_class, obj_qty,
                               object_ids, analy, TRUE );
                break;
            }
        }
        else
        {
            elem_get_minmax( resultElem, 0, analy );
        }
    }
    else if ( !p_result->origin.is_node_result )
    {
        switch ( p_result->superclasses[index] )
        {
        case G_UNIT:
            unit_get_minmax( resultElem, analy );
            break;
        case G_MAT:
            mat_get_minmax( resultElem, analy );
            break;
        case G_MESH:
            mesh_get_minmax( resultElem, analy );
            break;
        }
    }

    /* Free result query string */
    free(primals[0]);
}


#ifdef HAVE_TRANS_RES
/*****************************************************************
 * TAG( trans_result )
 *
 * Translation table which associates a result type with the command
 * string used to identify that result type.
 *
 * Each row in the table contains:
 *
 *     Result ID (int)
 *     Result title (string)
 *     Function which computes the result (function)
 *         its arguments are: (analy, result_array)
 *     Command string for displaying result (string)
 */
char *trans_result[][4] =
{
    {
        (char *) VAL_NONE,                        "No Result",
        (char *) NULL,                            "mat"
    },

    {
        (char *) VAL_NODE_DISPX,                  "X Displacement",
        (char *) compute_node_displacement,       "dispx"
    },
    {
        (char *) VAL_NODE_DISPY,                  "Y Displacement",
        (char *) compute_node_displacement,       "dispy"
    },
    {
        (char *) VAL_NODE_DISPZ,                  "Z Displacement",
        (char *) compute_node_displacement,       "dispz"
    },
    {
        (char *) VAL_NODE_DISPR,                  "XY Radial Displacement",
        (char *) compute_node_radial_displacement,"dispr"
    },
    {
        (char *) VAL_NODE_DISPMAG,                "Displacement Magnitude",
        (char *) compute_node_displacement,       "dispmag"
    },
    {
        (char *) VAL_NODE_VELX,                   "X Velocity",
        (char *) compute_node_velocity,           "velx"
    },
    {
        (char *) VAL_NODE_VELY,                   "Y Velocity",
        (char *) compute_node_velocity,           "vely"
    },
    {
        (char *) VAL_NODE_VELZ,                   "Z Velocity",
        (char *) compute_node_velocity,           "velz"
    },
    {
        (char *) VAL_NODE_VELMAG,                 "Velocity Magnitude",
        (char *) compute_node_velocity,           "velmag"
    },
    {
        (char *) VAL_NODE_ACCX,                   "X Acceleration",
        (char *) compute_node_acceleration,       "accx"
    },
    {
        (char *) VAL_NODE_ACCY,                   "Y Acceleration",
        (char *) compute_node_acceleration,       "accy"
    },
    {
        (char *) VAL_NODE_ACCZ,                   "Z Acceleration",
        (char *) compute_node_acceleration,       "accz"
    },
    {
        (char *) VAL_NODE_ACCMAG,                 "Acceleration Magnitude",
        (char *) compute_node_acceleration,       "accmag"
    },
    {
        (char *) VAL_NODE_TEMP,                   "Temperature",
        (char *) compute_node_temperature,        "temp"
    },
    {
        (char *) VAL_NODE_PINTENSE,               "Pressure Intensity",
        (char *) compute_node_pr_intense,         "pint"
    },
    {
        (char *) VAL_NODE_HELICITY,               "Helicity",
        (char *) compute_node_helicity,           "hel"
    },
    {
        (char *) VAL_NODE_ENSTROPHY,              "Enstrophy",
        (char *) compute_node_enstrophy,          "ens"
    },
    {
        (char *) VAL_NODE_K,                      "k",
        (char *) compute_node_in_data,            "k"
    },
    {
        (char *) VAL_NODE_EPSILON,                "Epsilon",
        (char *) compute_node_in_data,            "eps"
    },
    {
        (char *) VAL_NODE_A2,                     "A2",
        (char *) compute_node_in_data,            "a2"
    },
    {
        (char *) VAL_PROJECTED_VEC,               "Projected Vector Magnitude",
        (char *) compute_vector_component,        "pvmag"
    },

    {
        (char *) VAL_HEX_EPS_PD1,                 "Prin Dev Strain 1",
        (char *) compute_hex_strain,              "pdstrn1"
    },
    {
        (char *) VAL_HEX_EPS_PD2,                 "Prin Dev Strain 2",
        (char *) compute_hex_strain,              "pdstrn2"
    },
    {
        (char *) VAL_HEX_EPS_PD3,                 "Prin Dev Strain 3",
        (char *) compute_hex_strain,              "pdstrn3"
    },
    {
        (char *) VAL_HEX_EPS_MAX_SHEAR,           "Maximum Shear Strain",
        (char *) compute_hex_strain,              "pshrstr"
    },
    {
        (char *) VAL_HEX_EPS_P1,                  "Principal Strain 1",
        (char *) compute_hex_strain,              "pstrn1"
    },
    {
        (char *) VAL_HEX_EPS_P2,                  "Principal Strain 2",
        (char *) compute_hex_strain,              "pstrn2"
    },
    {
        (char *) VAL_HEX_EPS_P3,                  "Principal Strain 3",
        (char *) compute_hex_strain,              "pstrn3"
    },
    {
        (char *) VAL_HEX_RELVOL,                  "Relative Volume",
        (char *) compute_hex_relative_volume,     "relvol"
    },
    {
        (char *) VAL_HEX_VOL_STRAIN,              "Volumetric Strain",
        (char *) compute_hex_relative_volume,     "evol"
    },

    {
        (char *) VAL_SHELL_RES1,                  "M_xx Bending Resultant",
        (char *) compute_shell_in_data,           "res1"
    },
    {
        (char *) VAL_SHELL_RES2,                  "M_yy Bending Resultant",
        (char *) compute_shell_in_data,           "res2"
    },
    {
        (char *) VAL_SHELL_RES3,                  "M_xy Bending Resultant",
        (char *) compute_shell_in_data,           "res3"
    },
    {
        (char *) VAL_SHELL_RES4,                  "Q_xx Shear Resultant",
        (char *) compute_shell_in_data,           "res4"
    },
    {
        (char *) VAL_SHELL_RES5,                  "Q_yy Shear Resultant",
        (char *) compute_shell_in_data,           "res5"
    },
    {
        (char *) VAL_SHELL_RES6,                  "N_xx Normal Resultant",
        (char *) compute_shell_in_data,           "res6"
    },
    {
        (char *) VAL_SHELL_RES7,                  "N_yy Normal Resultant",
        (char *) compute_shell_in_data,           "res7"
    },
    {
        (char *) VAL_SHELL_RES8,                  "N_xy Normal Resultant",
        (char *) compute_shell_in_data,           "res8"
    },
    {
        (char *) VAL_SHELL_THICKNESS,             "Thickness",
        (char *) compute_shell_in_data,           "thick"
    },
    {
        (char *) VAL_SHELL_INT_ENG,               "Internal Energy",
        (char *) compute_shell_in_data,           "inteng"
    },

    {
        (char *) VAL_SHELL_SURF1,                 "Surface Stress 1",
        (char *) compute_shell_surface_stress,    "surf1"
    },
    {
        (char *) VAL_SHELL_SURF2,                 "Surface Stress 2",
        (char *) compute_shell_surface_stress,    "surf2"
    },
    {
        (char *) VAL_SHELL_SURF3,                 "Surface Stress 3",
        (char *) compute_shell_surface_stress,    "surf3"
    },
    {
        (char *) VAL_SHELL_SURF4,                 "Surface Stress 4",
        (char *) compute_shell_surface_stress,    "surf4"
    },
    {
        (char *) VAL_SHELL_SURF5,                 "Surface Stress 5",
        (char *) compute_shell_surface_stress,    "surf5"
    },
    {
        (char *) VAL_SHELL_SURF6,                 "Surface Stress 6",
        (char *) compute_shell_surface_stress,    "surf6"
    },
    {
        (char *) VAL_SHELL_EFF1,          "Effective Upper Surface Stress",
        (char *) compute_shell_surface_stress,    "eff1"
    },
    {
        (char *) VAL_SHELL_EFF2,          "Effective Lower Surface Stress",
        (char *) compute_shell_surface_stress,    "eff2"
    },
    {
        (char *) VAL_SHELL_EFF3,          "Maximum Effective Surface Stress",
        (char *) compute_shell_surface_stress,    "effmax"
    },

    {
        (char *) VAL_SHARE_SIGX,                  "X Stress",
        (char *) compute_share_stress,            "sx"
    },
    {
        (char *) VAL_SHARE_SIGY,                  "Y Stress",
        (char *) compute_share_stress,            "sy"
    },
    {
        (char *) VAL_SHARE_SIGZ,                  "Z Stress",
        (char *) compute_share_stress,            "sz"
    },
    {
        (char *) VAL_SHARE_SIGXY,                 "XY Stress",
        (char *) compute_share_stress,            "sxy"
    },
    {
        (char *) VAL_SHARE_SIGYZ,                 "YZ Stress",
        (char *) compute_share_stress,            "syz"
    },
    {
        (char *) VAL_SHARE_SIGZX,                 "ZX Stress",
        (char *) compute_share_stress,            "szx"
    },

    {
        (char *) VAL_SHARE_SIG_EFF,               "Effective Stress",
        (char *) compute_share_effstress,         "seff"
    },

    {
        (char *) VAL_SHARE_SIG_PD1,               "Prin Dev Stress 1",
        (char *) compute_share_prin_stress,       "pdev1"
    },
    {
        (char *) VAL_SHARE_SIG_PD2,               "Prin Dev Stress 2",
        (char *) compute_share_prin_stress,       "pdev2"
    },
    {
        (char *) VAL_SHARE_SIG_PD3,               "Prin Dev Stress 3",
        (char *) compute_share_prin_stress,       "pdev3"
    },
    {
        (char *) VAL_SHARE_SIG_MAX_SHEAR,         "Maximum Shear Stress",
        (char *) compute_share_prin_stress,       "maxshr"
    },
    {
        (char *) VAL_SHARE_SIG_P1,                "Principal Stress 1",
        (char *) compute_share_prin_stress,       "prin1"
    },
    {
        (char *) VAL_SHARE_SIG_P2,                "Principal Stress 2",
        (char *) compute_share_prin_stress,       "prin2"
    },
    {
        (char *) VAL_SHARE_SIG_P3,                "Principal Stress 3",
        (char *) compute_share_prin_stress,       "prin3"
    },

    {
        (char *) VAL_SHARE_EPSX,                  "X Strain",
        (char *) compute_share_strain,            "ex"
    },
    {
        (char *) VAL_SHARE_EPSY,                  "Y Strain",
        (char *) compute_share_strain,            "ey"
    },
    {
        (char *) VAL_SHARE_EPSZ,                  "Z Strain",
        (char *) compute_share_strain,            "ez"
    },
    {
        (char *) VAL_SHARE_EPSXY,                 "XY Strain",
        (char *) compute_share_strain,            "exy"
    },
    {
        (char *) VAL_SHARE_EPSYZ,                 "YZ Strain",
        (char *) compute_share_strain,            "eyz"
    },
    {
        (char *) VAL_SHARE_EPSZX,                 "ZX Strain",
        (char *) compute_share_strain,            "ezx"
    },

    {
        (char *) VAL_SHARE_EPS_EFF,               "Eff Plastic Strain",
        (char *) compute_share_eff_strain,        "eeff"
    },

    {
        (char *) VAL_SHARE_PRESS,                 "Pressure",
        (char *) compute_share_press,             "press"
    },

    {
        (char *) VAL_BEAM_AX_FORCE,               "Axial Force",
        (char *) compute_beam_in_data,            "axfor"
    },
    {
        (char *) VAL_BEAM_S_SHEAR,                "S Shear Resultant",
        (char *) compute_beam_in_data,            "sshear"
    },
    {
        (char *) VAL_BEAM_T_SHEAR,                "T Shear Resultant",
        (char *) compute_beam_in_data,            "tshear"
    },
    {
        (char *) VAL_BEAM_S_MOMENT,               "S Moment",
        (char *) compute_beam_in_data,            "smom"
    },
    {
        (char *) VAL_BEAM_T_MOMENT,               "T Moment",
        (char *) compute_beam_in_data,            "tmom"
    },
    {
        (char *) VAL_BEAM_TOR_MOMENT,             "Torsional Resultant",
        (char *) compute_beam_in_data,            "tor"
    },
    {
        (char *) VAL_BEAM_S_AX_STRN_P,            "S Axial Strain (+)",
        (char *) compute_beam_axial_strain,       "saep"
    },
    {
        (char *) VAL_BEAM_S_AX_STRN_M,            "S Axial Strain (-)",
        (char *) compute_beam_axial_strain,       "saem"
    },
    {
        (char *) VAL_BEAM_T_AX_STRN_P,            "T Axial Strain (+)",
        (char *) compute_beam_axial_strain,       "taep"
    },
    {
        (char *) VAL_BEAM_T_AX_STRN_M,            "T Axial Strain (-)",
        (char *) compute_beam_axial_strain,       "taem"
    },

    {
        (char *) VAL_TRUSS_AX_FORCE,               "Axial Force",
        (char *) compute_truss_in_data,            "axfor"
    },
    {
        (char *) VAL_TRUSS_S_SHEAR,                "S Shear Resultant",
        (char *) compute_truss_in_data,            "sshear"
    },
    {
        (char *) VAL_TRUSS_T_SHEAR,                "T Shear Resultant",
        (char *) compute_truss_in_data,            "tshear"
    },
    {
        (char *) VAL_TRUSS_S_MOMENT,               "S Moment",
        (char *) compute_truss_in_data,            "smom"
    },
    {
        (char *) VAL_TRUSS_T_MOMENT,               "T Moment",
        (char *) compute_truss_in_data,            "tmom"
    },
    {
        (char *) VAL_TRUSS_TOR_MOMENT,             "Torsional Resultant",
        (char *) compute_truss_in_data,            "tor"
    },
    {
        (char *) VAL_TRUSS_S_AX_STRN_P,            "S Axial Strain (+)",
        (char *) compute_truss_axial_strain,       "saep"
    },
    {
        (char *) VAL_TRUSS_S_AX_STRN_M,            "S Axial Strain (-)",
        (char *) compute_truss_axial_strain,       "saem"
    },
    {
        (char *) VAL_TRUSS_T_AX_STRN_P,            "T Axial Strain (+)",
        (char *) compute_truss_axial_strain,       "taep"
    },
    {
        (char *) VAL_TRUSS_T_AX_STRN_M,            "T Axial Strain (-)",
        (char *) compute_truss_axial_strain,       "taem"
    },

    {
        (char *) NULL,                    NULL,
        (char *) NULL,                    NULL
    }   /* Dummy to signal end. */
};


/*****************************************************************
 * TAG( resultid_to_index )
 *
 * Table which gives the index in the trans_result table for each
 * result id.
 */
int *resultid_to_index;


/*****************************************************************
 * TAG( init_resultid_table )
 *
 * Create the resultid_to_index table.
 */
void
init_resultid_table()
{
    int i;

    resultid_to_index = NEW_N( int, VAL_ALL_END, "Resultid table" );

    /* Initialize to -1 to keep track of unmapped IDs. */
    for ( i = 0; i < VAL_ALL_END; i++ )
        resultid_to_index[i] = -1;

    for ( i = 0; trans_result[i][3] != NULL; i++ )
        resultid_to_index[(int)trans_result[i][0]] = i;
}


/*****************************************************************
 * TAG( lookup_result_id )
 *
 * Returns the result ID number for the given result command
 * name.  If the name is not found in the results table, the
 * routine returns -1.
 */
int
lookup_result_id( char *com_name )
{
    int i;

    /* Get the result type from the command using the translation table. */
    for ( i = 0; trans_result[i][3] != NULL; i++ )
    {
        /* For efficiency, match the first characters first. */
        if ( com_name[0] == trans_result[i][3][0] &&
                strcmp( com_name, trans_result[i][3] ) == 0 )
        {
            return( (int)trans_result[i][0] );
        }
    }

    return( -1 );
}


/*****************************************************************
 * TAG( is_derived )
 *
 * Returns TRUE if the specified result type can be computed from
 * the variables in the plotfile database, otherwise FALSE.
 */
int
is_derived( Result_type result_id )
{
    if ( resultid_to_index[result_id] < 0 )
        return FALSE;
    else
        return TRUE;
}


/*****************************************************************
 * TAG( is_hex_result )
 *
 * Returns TRUE if the specified result type applies to hex
 * elements.
 */
int
is_hex_result( Result_type result_id )
{
    if ( result_id > VAL_HEX_BEGIN && result_id < VAL_HEX_END )
        return TRUE;
    else
        return FALSE;
}


/*****************************************************************
 * TAG( is_shell_result )
 *
 * Returns TRUE if the specified result type applies to shell
 * elements.
 */
int
is_shell_result( Result_type result_id )
{
    if ( result_id > VAL_SHELL_BEGIN && result_id < VAL_SHELL_END )
        return TRUE;
    else
        return FALSE;
}


/*****************************************************************
 * TAG( is_shared_result )
 *
 * Returns TRUE if the specified result type applies to brick and
 * shell elements.
 */
int
is_shared_result( Result_type result_id )
{
    if ( result_id > VAL_SHARE_BEGIN && result_id < VAL_SHARE_END )
        return TRUE;
    else
        return FALSE;
}


/*****************************************************************
 * TAG( is_beam_result )
 *
 * Returns TRUE if the specified result type applies to beam
 * elements.
 */
int
is_beam_result( Result_type result_id )
{
    if ( result_id > VAL_BEAM_BEGIN && result_id < VAL_BEAM_END )
        return TRUE;
    else
        return FALSE;
}


/*****************************************************************
 * TAG( is_nodal_result )
 *
 * Returns TRUE if the specified result type is nodal data.
 */
int
is_nodal_result( Result_type result_id )
{
    if ( result_id > VAL_NODE_BEGIN && result_id < VAL_NODE_END )
        return TRUE;
    else
        return FALSE;
}

#endif


#ifdef GRIZ_SERVER_BUILD
/*****************************************************************
 * TAG( server_build_results_data )
 *
 * Build a cJSON object with { results: [...], current: ... } from
 * the primal and derived result hash tables.
 */
static void
server_add_htable_results( void *arr_v, Hash_table *ht,
                           const char *origin_label )
{
    cJSON *arr = (cJSON *) arr_v;
    int bucket, i;
    Htable_entry *p_hte;

    if ( ht == NULL || ht->qty_entries == 0 )
        return;

    /* Walk all hash table buckets and entries.  The key is the result
     * name string.  For primal results, data points to a Primal_result
     * that has a long_name; for derived results, we use just the key. */
    for ( bucket = 0; bucket < ht->size; bucket++ )
    {
        for ( p_hte = ht->table[bucket]; p_hte != NULL; p_hte = p_hte->next )
        {
            cJSON *entry;
            const char *title = NULL;

            if ( p_hte->key == NULL )
                continue;

            /* Try to get a descriptive title from the Primal_result. */
            if ( p_hte->data != NULL )
            {
                Primal_result *pr = (Primal_result *) p_hte->data;
                if ( pr->long_name != NULL && pr->long_name[0] != '\0' )
                    title = pr->long_name;
            }

            entry = cJSON_CreateObject();
            cJSON_AddStringToObject( entry, "name", p_hte->key );
            cJSON_AddStringToObject( entry, "title",
                                     title ? title : p_hte->key );
            cJSON_AddStringToObject( entry, "origin", origin_label );
            cJSON_AddItemToArray( arr, entry );
        }
    }
}

void *
server_build_results_from_htable( void *arr_v, void *ht_v,
                                  const char *origin_label )
{
    server_add_htable_results( arr_v, (Hash_table *) ht_v, origin_label );
    return arr_v;
}
#endif /* GRIZ_SERVER_BUILD */


/*****************************************************************
 * TAG( check_nodal_result )
 *
 * Used for debugging updates to the nodal result buffer.
 */
void
check_nodal_result( Analysis *a )
{
    float *data;
    int i, qty;
    double sum=0.0;

    return; /* Remove when debugging */

    qty = a->mesh_table[a->cur_mesh_id].node_geom->qty ;
    data = a->mesh_table[a->cur_mesh_id].node_geom->data_buffer;

    for (i=0;
            i<qty;
            i++)
        sum += data[i];

    return;
}

/*****************************************************************
 * TAG( get_result_qty )
 *
 * Returns the quantity of a result for a given subrecord id.
 *
 */
int
get_result_qty( Analysis *analy, int subrec_id )
{
    Result *p_result;
    int index;
    int subrec, srec;
    Subrec_obj *p_subrec;
    p_result = analy->cur_result;
    index    = analy->result_index;
    subrec   = p_result->subrecs[index];
    srec     = p_result->srec_id;
    p_subrec = analy->srec_tree[srec].subrecs + subrec;
    return (p_subrec->subrec.qty_objects);
}

/*****************************************************************
 * TAG( get_element_set_id )
 *
 * Returns the element set (es) id from the es name.
 *
 */
int
get_element_set_id( char *es_input_ptr )
{
    int i=0;
    int es_id=-1;
    char *es_ptr=NULL, es_temp[64]="";

    es_ptr = strstr( es_input_ptr, "es_" );
    es_ptr += 3;
    if ( es_ptr )
    {
        strcpy( es_temp, es_ptr );
        for ( i=0;
                i<strlen( es_temp );
                i++)
        {
            if ( !isdigit(es_temp[i] ) )
            {
                es_temp[i]='\0';
                break;
            }
        }
        sscanf( es_temp, "%d", &es_id );
    }

    return( es_id );
}

/*****************************************************************
 * TAG( get_element_set_index )
 *
 * Returns the element set (es) index for the specified es id.
 *
 */
//int
//get_element_set_index( Analysis *analy, int es_id )
//{
//    int i=0;
//    int es_index=-1;
//
//    for (i=0;
//            i<analy->es_cnt;
//            i++ )
//    {
//        if ( analy->es_intpoints[i].es_id == es_id )
//        {
//            es_index = i;
//            break;
//        }
//    }
//    return( es_index );
//}

/*****************************************************************
 * TAG( get_intpoint_index )
 *
 * Returns the index of the specified label or -1 if not found.
 *
 */
int
get_intpoint_index ( int label, int num_labels, int *intpoint_labels )
{
    int i=0;
    int label_index=-1;

    for ( i=0;
            i<num_labels;
            i++ )
    {
        if ( intpoint_labels[i]==label )
            label_index = i;
    }
    return ( label_index );
}

/*****************************************************************
 * TAG( get_intpoints )
 *
 * Returns the current integration points in array
 * intpoints for a specified element set id.
 *
 */
//void
//get_intpoints ( Analysis *analy, int es_id, int intpoints[3] )
//{
//    int i;
//
//    for ( i=0;
//            i<analy->es_cnt;
//            i++ )
//    {
//        if ( es_id == analy->es_intpoints[i].es_id )
//        {
//            intpoints[0] = analy->es_intpoints[i].in_mid_out_set[0];
//            intpoints[1] = analy->es_intpoints[i].in_mid_out_set[1];
//            intpoints[2] = analy->es_intpoints[i].in_mid_out_set[2];
//            return;
//        }
//    }
//    intpoints[0] =  intpoints[1] =  intpoints[2] = -1;
//    return;
//}

/*****************************************************************
 * TAG( set_default_intpoints )
 *
 * Returns the default integration points in array
 * default_intpoints.
 *
 */
void
set_default_intpoints ( int num_intpoints, int num_labels,
                        int *intpoint_labels, int default_intpoints[3] )
{
    int i;
    int calc_middle=0, middle_point=-1;

    default_intpoints[0] =  default_intpoints[1] = default_intpoints[2] = -1;

    if ( intpoint_labels[0] == 1 )
        default_intpoints[0] = 1; /* Inner */

    /* Calculate the MIDDLE integration point */
    calc_middle = num_intpoints/2.0;
    if ( calc_middle*2 != num_intpoints )   /* Odd number of points */
    {
        calc_middle++;

        /* Make sure the middle point is in the list of labels */
        for ( i=0;
                i<num_labels-1;
                i++ )
        {
            if ( intpoint_labels[i] == calc_middle )
                middle_point = calc_middle;
        }

    }
    default_intpoints[1] = middle_point; /* MIDDLE integration point undefined if -1 */
    if ( intpoint_labels[num_labels-1] == num_intpoints )
        default_intpoints[2] = num_intpoints; /* Outer */
}


/*****************************************************************
 * TAG( create_result )
 *
 * Returns a linked list of result pointers 
 *
 */

Result * create_result_list( char * token, Analysis *analy)
{
    char * class_names[2000];
    char name[64];
    int super_classes[2000];
    int super_classes_found[QTY_SCLASS];
    int status, qty_classes;
    int i, j, k, l, n, rval, copied;
    int *subrecs;
    char ipt[125];
    char raw_primal[64];
    int qty_candidates;
    Result * res_ptr = NULL;
    Result * p1;
    Result * p2;
    Result result;
    Result_candidate *p_rc;
    Result_candidate *p_es_rc;
    Bool_type raw = TRUE;
    Bool_type match = FALSE;

    for(i = 0; i < QTY_SCLASS; i++)
    {
        super_classes_found[i] = 0;
    } 
    status = mili_get_class_names(analy, &qty_classes, class_names, super_classes);
    if(status != 0)
    {
        return NULL;
    } 
   
    /* allocate for a 1 based array so that the subrec number equals the indes into the array */
    subrecs = (int *) calloc(analy->srec_tree->qty + 1, sizeof(int));
    if(subrecs == NULL)
    {
        popup_dialog(WARNING_POPUP, "Out of memory in function create_result_list. exiting\n");
        parse_command("quit", analy);
    }

    for(i = 0; possible_results[i].valid_superclasses[0] != -1; i++)
    {
        p_rc = &possible_results[i];
        for(j = 0; p_rc->short_names[j] != NULL; j++)
        {
            if(strcmp(token, p_rc->short_names[j]) == 0)
            {
                init_result(&result);
                rval = find_result(analy, analy->result_source, TRUE, &result, token);
                if(rval)
                {
                    if(subrecs[result.subrecs[0]] == 0)
                    {
                        res_ptr = insert_result(res_ptr, result, analy);
                        for(k = 0; k < result.qty; k++)
                        {
                            subrecs[result.subrecs[k]] = 1;
                        } 
                   } 
                }
               
            }
        }
    }

    init_result(&result);
 
    rval = find_result(analy, analy->result_source, TRUE, &result, token);
    if( rval == 1 ){
        /* ok element sets are not in this plot file */
        if(raw == TRUE)
        {
            if(subrecs[result.subrecs[0]] == 0)
            {
                res_ptr = insert_result(res_ptr, result, analy);
            }
        } else
        {
            init_result(&result);
            rval = find_result(analy, analy->result_source, TRUE, &result, raw_primal);
            if(rval == TRUE)
            {
                if(subrecs[result.subrecs[0]] == 0)
                {
                    res_ptr = insert_result(res_ptr, result, analy);
                }
            } else
            {
                init_result(&result);
                rval = find_result(analy, analy->result_source, TRUE, &result, token);
                if(rval == 1)
                {
                    if(subrecs[result.subrecs[0]] == 0)
                    {
                        res_ptr = insert_result(res_ptr, result, analy);
                    }
                }
            }
        } 
    }
   
    free(subrecs); 
    return res_ptr;        
}

/*****************************************************************
 * TAG( insert_result )
 *
 * creates a linked list of results or if one is already created
 * adds a result to the end of the linked list 
 *
 */
Result * insert_result(Result * res_ptr, Result result, Analysis * analy)
{
    Result *p1;
    Result *p2;
 
    p2 = (Result *) malloc(sizeof(Result));
    if(p2 == NULL)
    {
        popup_dialog(WARNING_POPUP, "Out of memory in function insert result. Exiting\n");
        parse_command("quit", analy);
    }

    *p2 = result;
    if(res_ptr == NULL)
    {
        res_ptr = p2;
        res_ptr->prev = NULL;
        res_ptr->next = NULL;
    } else
    {
        if(res_ptr->next == NULL)
        {
            res_ptr->next = p2;
            p2->prev = res_ptr;
            p2->next = NULL;
        } else
        {
            for(p1 = res_ptr; p1->next != NULL; p1 = p1->next);
            p1->next = p2;
            p2->prev = p1;
            p2->next = NULL;
        }
    }

    return res_ptr;
}


/*****************************************************************
 * TAG( delete_result_list )
 *
 * Takes a pointer to a linked list of results and frees up the 
 * memory and sets the pointer to NULL
 *
 */

void delete_result_list(Result ** res_ptr, Analysis * analy)
{
   Result *p1;
   Result *p2;

   if(*res_ptr == NULL)
   {
       return;
   }
   for(p1 = *res_ptr; p1->next != NULL; p1 = p1->next);

   for(p2 = p1->prev; p2 != NULL; p2 = p2->prev)
   {
       p1->prev = NULL;
       p2->next = NULL;
       free(p1);
       p1 = NULL;
       p1 = p2;
   }

   free(p1);
   p1 = NULL;
   *res_ptr = NULL;

}
