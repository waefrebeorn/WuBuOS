#!/usr/bin/env python3
"""One-shot image -> 3-D mesh for the WuBu Buddy (Hunyuan3D-2 shape pass).

This is the single expensive step of the buddy pipeline: turn ONE clean,
front-facing, full-body reference pose into real geometry.  Everything after it
(retopology, rigging, IK, animation, DKC rendering) is done locally in Blender
with no further cost, so the model choice matters more than iteration count.

Shape pass only -- no texture painter: we shade with our own brand materials and
render the DKC sprites ourselves, and skipping texgen is what makes it fit in
this laptop's 8 GB of VRAM.

Run:
  ~/opt/h3d/bin/python tools/buddy_gen3d.py IN.png OUT.glb \
      [--model tencent/Hunyuan3D-2] [--octree 256] [--steps 30]

octree_resolution trades mesh density for VRAM: 384 is the default (~10 GB),
256 is the safe setting for an 8 GB card.
"""
import argparse
import os
import sys

REPO = os.environ.get("HY3DGEN_REPO", os.path.expanduser("~/opt/Hunyuan3D-2"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("out")
    ap.add_argument("--model", default="tencent/Hunyuan3D-2")
    ap.add_argument("--octree", type=int, default=256,
                    help="marching-cubes grid; 256 fits 8GB, 384 is default")
    ap.add_argument("--steps", type=int, default=30)
    ap.add_argument("--offload", choices=("none", "cpu"), default="none",
                    help="'cpu' moves the VAE between passes to save VRAM")
    a = ap.parse_args()

    if REPO not in sys.path:
        sys.path.insert(0, REPO)

    import torch
    from PIL import Image
    from hy3dgen.shapegen import Hunyuan3DDiTFlowMatchingPipeline

    if not torch.cuda.is_available():
        sys.stderr.write("CUDA unavailable -- this needs the GPU.\n")
        return 3

    img = Image.open(a.image).convert("RGBA")
    free, total = torch.cuda.mem_get_info()
    print(f"VRAM free {free/2**30:.2f} GiB / {total/2**30:.2f} GiB")

    pipe = Hunyuan3DDiTFlowMatchingPipeline.from_pretrained(a.model)
    if a.offload == "cpu":
        try:
            pipe.model.enable_model_cpu_offload()
        except Exception as e:
            print(f"(cpu offload unavailable: {e})")

    mesh = pipe(image=img, num_inference_steps=a.steps,
                octree_resolution=a.octree)[0]
    mesh.export(a.out)
    n_v = len(mesh.vertices) if hasattr(mesh, "vertices") else "?"
    print(f"exported {a.out} (vertices={n_v})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
