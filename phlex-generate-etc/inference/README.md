This directory demonstrates running compressed/rasterized images through the model trained within https://github.com/nuRiceLab/dune-tech_PHLEX_India/blob/feature/calcuttj-inference/tutorial-ml/ML-03-CNN.ipynb. 

`inference.jsonnet` operates in the following order:
1. Generating a single particle with a particle gun
2. Tracking and observable gathering via g4/edep-sim within a cube of liquid argon
3. Creating an image by voxelizing the depositions within the volume then summing over the y dimension: `rasterize.cpp`
4. Sending that through the model stored in `traced.ptrom`

Note: the images used to train this model were made in some other way, so there might be a `domain shift` between the training images and those made from the simulation here
