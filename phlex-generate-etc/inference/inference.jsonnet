{
  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { parent: 'job', total: 4, starting_number: 1 },
    },
  },
  sources: {
    // Python generator: provides a particle table (creator 'pygen', suffix 'particles')
    gun: {
      cpp: 'hmp_gen_event_gun',
      output_layer: 'event',
      pdg: 11,
      mass: 0.511,         // MeV
      energy: 500.0,         // MeV kinetic
      direction: [0, 0, 1],
      position: [0, 0, -500],  // mm
      number: 1,
      momentum_unit: 'MeV',
      length_unit: 'mm',
    },
  },
  modules: {

    edep_sim_tracking: {
      cpp: 'esp_tracking',
      input_layer: 'event',
      gdml: '../../transport/gdml/minilar.gdml',
      ## This turns off the CPU-heavy photon simulation
      macro: '/process/optical/scintillation/setStackPhotons false\n',
    },
    edep_observables: {
     cpp: 'esp_observables',
     input_layer: 'event',
     input_from: 'edep_sim_tracking',  // consumes the TG4Event
    },
    // Segments -> 2D image (creator 'rasterize', suffix 'image'): energy
    // voxelized in 3D, then summed along y.  Grid in mm; defaults shown.
    rasterize: {
      cpp: 'rasterize',
      input_layer: 'event',
      voxel_size: 1000.0 / 28,
      x_min: -500, x_max: 500,
      y_min: -500, y_max: 500,
      z_min: -500, z_max: 500,
      weight: 'energy_deposit',  // or another double column, e.g. n_electrons
    },
    // image_for_py: {
    //   cpp: 'image_for_python',
    //   input_layer: 'event',
    // },
    inference: {
      py: 'inference',
      input: [{creator: 'rasterize', suffix: 'py_image', layer: 'event'}],
      input_shape: [1,28,28],
      model_file: '/home/jacob/phlex-work/dune-tech_PHLEX_India/phlex-generate-etc/inference/traced.ptrom',
      output: ['particle_class'],  // a list: a bare string is read as one suffix per character
    },
    // Write data product to output file
    output: {
      cpp: 'form_module',
      products: ['image', 'particle_class'],
    },
    // hdf_out: {
    //  cpp: 'phlex_arrow_hdf_output',
    //  output_file: 'obs.h5',  // writes all Arrow-typed products
    // },
  },
}