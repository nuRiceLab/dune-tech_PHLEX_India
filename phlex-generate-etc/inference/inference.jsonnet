{
  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { parent: 'job', total: 4, starting_number: 1 },
    },
  },
  sources: {
    // Python generator: provides a particle table (creator 'pygen', suffix 'particles')
    pygen: {
      py: 'ccqe_generator', ##TODO -- REPLACE THIS WITH THE SKELETON
      layer: 'event',
      muon_angle: 45,
      positions: ['-180 300 115']
      # You may add configuration here if you implement that within your generator
    },
  },
  modules: {
    // Particle table -> HepMC3::GenEvent (creator 'input', suffix 'genevent')
    assemble: {
      cpp: 'genevent_from_table',
      input_layer: 'event',
    },
    edep_sim_tracking: {
      cpp: 'esp_tracking',
      input_layer: 'event',
      gdml: 'gdml/pdhd_like.gdml',
      ## This turns off the CPU-heavy photon simulation
      macro: '/process/optical/scintillation/setStackPhotons false\n',
    },
    edep_observables: {
     cpp: 'esp_observables',
     input_layer: 'event',
     input_from: 'edep_sim_tracking',  // consumes the TG4Event
    },
    hdf_out: {
     cpp: 'phlex_arrow_hdf_output',
     output_file: 'obs.h5',  // writes all Arrow-typed products
    },
  },
}