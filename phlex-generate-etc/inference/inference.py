import numpy as np
import numpy.typing as npt
import torch

class ModelRunner():
    def __init__(self, model_file: str, input_shape: tuple[int], device: str = 'cpu'):
        self.device = device if torch.cuda.is_available() else 'cpu'
        self.model = torch.jit.load(model_file).to(self.device)
        self.input_shape = input_shape
        self.dtype = torch.float32



    def inference(self, image: npt.NDArray[np.float64]) -> int:

        #Todo -- check shape matches
        image_tensor = torch.tensor(
            image.reshape(*self.input_shape)
        ).to(self.device).to(self.dtype).reshape(1,*self.input_shape) #add batch dim

        result = self.model(image_tensor)
        return int(result[0].argmax())


def PHLEX_REGISTER_ALGORITHMS(m, config):
    input_shape = config['input_shape']
    model_file = config['model_file']
    mr = ModelRunner(
        model_file, input_shape,
    )

    m.transform(
        mr.inference, input_family=config['input'],
        output_product_suffixes=config['output']
    )

