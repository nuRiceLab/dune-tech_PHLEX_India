## Introduction
This directory demonstrates running compressed/rasterized images through the model trained within https://github.com/nuRiceLab/dune-tech_PHLEX_India/blob/feature/calcuttj-inference/tutorial-ml/ML-03-CNN.ipynb. 

`inference.jsonnet` operates in the following order:
1. Generating a single particle with a particle gun
2. Tracking and observable gathering via g4/edep-sim within a cube of liquid argon
3. Creating an image by voxelizing the depositions within the volume then summing over the y dimension: `rasterize.cpp`
4. Sending that through the model stored in `traced.ptrom`: `inference.py`

Note: the images used to train this model were made in some other way which at the time of writing is still unknown: so the model won't perform well on these events. It still shows how inference can be integrated within phlex.


## Building
You should run `build.sh` in the dir above to checkout and build an edited version of phlex-examples
`mkdir build && cmake . -B build -G Ninja && ninja -C build`

## Running
Make sure `../build-phlex-examples`, `/cvmfs/phaindia.example.org/env/phaindia/view/lib64/`, and `build` are in your PHLEX_PLUGIN_PATH

`export PHLEX_PLUGIN_PATH=/path/to/build-phlex-examples:${PWD}/build:${PHLEX_PLUGIN_PATH}`

Also make sure this dir is in your PYTHONPATH:

`export PYTHONPATH=${PWD}:${PYTHONPATH}`

now you can run from within build:

<pre>
cd build
phlex -c ../inference.jsonnet --stage infer
</pre>

## Output
An output FORM/TTree file is produced with 2 trees:
1. `root_ttree__rasterize:rasterize__infer` -- contains the image
2. `root_ttree__inference:inference__infer` -- contains the argmax of the output of the model (aka the predicted class)

You can visualize the images by running in python (within a venv with `uproot`, `numpy`, and `matplotlib` installed):
<pre>
  import uproot, numpy as np, matplotlib.pyplot as plt
  f = uproot.open('build/output.root')
  t = f['root_ttree__rasterize:rasterize__infer']
  image = t['image'].array(library='np')
  plt.imshow(image[0].reshape(28,28), aspect='auto'); plt.colorbar(); plt.show()
</pre>

<img width="668" height="592" alt="image" src="https://github.com/user-attachments/assets/0dc61cb5-cd09-471c-9b7a-3131b591b4d9" />


You can also look at the predicted class in python:
<pre>
  t_class = f['root_ttree__inference:inference__infer']
  classes = t_class['particle_class'].array(library='np')
</pre>

