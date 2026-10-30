"""Phlex source for the CCQE kinematics in ccqe_kinematics.py.

Generates one nu + n -> mu + p interaction per vertex, so an event can hold
several vertices, each with its outgoing muon and proton.  The event is
returned as a flat event table (see genevent_table.py), which the C++
transform genevent_from_table turns into a HepMC3::GenEvent.
"""

import numpy as np
import numpy.typing as npt

import phlex_config
from ccqe_kinematics import ccqe_kinematics, mmu, mp
from genevent_table import GenEventTable
from phlex import Variant


def generate(
    event_number: int, positions: list[tuple[float, float, float]], muon_angle: float
) -> npt.NDArray[np.float64]:
    """Build an event with one CCQE interaction at each vertex position.

    Args:
        event_number (int): Event number stored in the table.
        positions (list): Vertex positions (x, y, z) in cm.
        muon_angle (float): Muon angle to the neutrino direction, in degrees.

    Returns:
        numpy.ndarray: The event table described in genevent_table.py.
    """
    momenta = ccqe_kinematics(muon_angle)
    table = GenEventTable(event_number)
    for position in positions:
        print('ADDING POSITION', position)
        vtx = table.add_vertex(*position)
        table.add_particle(vtx, 13, 1, momenta["muon"], mmu)
        table.add_particle(vtx, 2212, 1, momenta["proton"], mp)
    
    return table.to_array()


def PHLEX_REGISTER_PROVIDERS(s, config):
    """Register the generator as a provider of the event table.

    Configuration:
        layer: data layer to provide events in.
        positions: vertex positions in cm, one string 'x y z' per vertex
            (default ['0 0 0']).  phlex cannot read nested arrays, hence strings.
        muon_angle: muon angle in degrees (default 100).
        creator, suffix: product names (defaults "pygen", "particles").

    Args:
        s (internal): Phlex source representation.
        config (internal): Phlex configuration representation.

    Returns:
        None
    """
    positions = [
        phlex_config.xyz(p) for p in phlex_config.get(config, "positions", ("0 0 0",))
    ]
    muon_angle = float(phlex_config.get(config, "muon_angle", 100.0))

    def provide(di: "data_cell_index") -> npt.NDArray[np.float64]:
        return generate(di.number(), positions, muon_angle)

    s.provide(
        provide,
        {
            "creator": phlex_config.get(config, "creator", "pygen"),
            "layer": config["layer"],
            "suffix": phlex_config.get(config, "suffix", "particles"),
        },
        "generate",
    )
