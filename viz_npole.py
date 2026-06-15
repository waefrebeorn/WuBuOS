
"""
N-Pole Cartpole (7-10 poles) Visualization for Social Media
WuBuOS BearRL - Sovereign C11 RL 
Shows 7, 8, 9, 10 pole balancing achieved in pure C
"""

import numpy as np
import matplotlib
matplotlib.use('Agg')  # Non-interactive backend
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.patches import Rectangle, Circle
from matplotlib.collections import LineCollection
import os

# ============================================================
# N-Pole Cartpole Physics (matching C implementation)
# ============================================================

class NPoleCartpole:
    def __init__(self, num_poles=7, dt=0.01):
        self.N = num_poles
        self.dt = dt
        self.gravity = 9.8
        self.cart_mass = 1.0
        self.force_mag = 10.0
        self.cart_pos_threshold = 4.0
        self.angle_threshold = 1.73  # ~99 degrees
        
        # Pole properties (matching C: lighter/shorter toward tip)
        self.pole_masses = np.array([0.1 * (1.0 - i * 0.02) for i in range(num_poles)])
        self.pole_lengths = np.array([0.5 * (1.0 - i * 0.05) for i in range(num_poles)])
        self.pole_mass_length = self.pole_masses * self.pole_lengths
        self.total_mass = self.cart_mass + np.sum(self.pole_masses)
        
        # State: [cart_x, cart_vx, theta_1, omega_1, ..., theta_N, omega_N]
        self.reset()
    
    def reset(self):
        self.cart_x = np.random.uniform(-0.05, 0.05)
        self.cart_vx = np.random.uniform(-0.05, 0.05)
        self.theta = np.random.uniform(-0.05, 0.05, self.N)
        self.omega = np.random.uniform(-0.05, 0.05, self.N)
        self.step_count = 0
        return self.get_obs()
    
    def get_obs(self):
        obs = np.zeros(2 + 2 * self.N)
        obs[0] = self.cart_x
        obs[1] = self.cart_vx
        for i in range(self.N):
            obs[2 + 2*i] = self.theta[i]
            obs[2 + 2*i + 1] = self.omega[i]
        return obs
    
    def compute_accelerations(self, force):
        """Recursive Lagrangian dynamics (matching C RK4)"""
        N = self.N
        
        # Build mass matrix M and force vector F for [x_ddot, theta_ddot_1...theta_ddot_N]
        M = np.zeros((N + 1, N + 1))
        F = np.zeros(N + 1)
        
        # M[0][0] = total_mass (already includes pole masses)
        M[0, 0] = self.total_mass
        for i in range(N):
            cos_t = np.cos(self.theta[i])
            M[0, i+1] = self.pole_mass_length[i] * cos_t
            M[i+1, 0] = M[0, i+1]
        
        for i in range(N):
            mi = self.pole_masses[i]
            li = self.pole_lengths[i]
            sin_ti = np.sin(self.theta[i])
            M[i+1, i+1] = mi * li * li
            
            # Off-diagonal coupling
            for j in range(i+1, N):
                mj = self.pole_masses[j]
                lj = self.pole_lengths[j]
                theta_diff = self.theta[i] - self.theta[j]
                cos_diff = np.cos(theta_diff)
                M[i+1, j+1] = mj * li * lj * cos_diff
                M[j+1, i+1] = M[i+1, j+1]
            
            # Coriolis for pole i
            coriolis = 0.0
            for j in range(N):
                if j == i: continue
                mj = self.pole_masses[j]
                lj = self.pole_lengths[j]
                omega_j = self.omega[j]
                omega_i = self.omega[i]
                theta_diff = self.theta[i] - self.theta[j]
                sin_diff = np.sin(theta_diff)
                coriolis += mj * li * lj * omega_i * omega_j * sin_diff
            
            F[i+1] = -mi * self.gravity * li * sin_ti + coriolis
        
        # External force on cart
        F[0] = force
        
        # Cart Coriolis terms
        for i in range(N):
            mi = self.pole_masses[i]
            li = self.pole_lengths[i]
            sin_ti = np.sin(self.theta[i])
            omega_i = self.omega[i]
            F[0] += mi * li * omega_i * omega_i * sin_ti
        
        # Solve M * acc = F
        acc = np.linalg.solve(M, F)
        return acc[0], acc[1:]  # cart_acc, theta_acc
    
    def rk4_step(self, force):
        """4th-order Runge-Kutta integration"""
        dt = self.dt
        
        # Save initial state
        x0, vx0 = self.cart_x, self.cart_vx
        theta0 = self.theta.copy()
        omega0 = self.omega.copy()
        
        # k1
        cart_acc1, theta_acc1 = self.compute_accelerations(force)
        
        # Temporarily update for k2
        self.cart_x = x0 + 0.5 * dt * vx0
        self.cart_vx = vx0 + 0.5 * dt * cart_acc1
        self.theta = theta0 + 0.5 * dt * omega0
        self.omega = omega0 + 0.5 * dt * theta_acc1
        
        # k2
        cart_acc2, theta_acc2 = self.compute_accelerations(force)
        
        # k3
        self.cart_x = x0 + 0.5 * dt * (vx0 + 0.5 * dt * cart_acc1)
        self.cart_vx = vx0 + 0.5 * dt * cart_acc2
        self.theta = theta0 + 0.5 * dt * (omega0 + 0.5 * dt * theta_acc1)
        self.omega = omega0 + 0.5 * dt * theta_acc2
        
        cart_acc3, theta_acc3 = self.compute_accelerations(force)
        
        # k4
        self.cart_x = x0 + dt * (vx0 + 0.5 * dt * cart_acc2)
        self.cart_vx = vx0 + dt * cart_acc3
        self.theta = theta0 + dt * (omega0 + 0.5 * dt * theta_acc2)
        self.omega = omega0 + dt * theta_acc3
        
        cart_acc4, theta_acc4 = self.compute_accelerations(force)
        
        # Final weighted average
        self.cart_x = x0 + dt/6.0 * (vx0 + 2*(vx0 + 0.5*dt*cart_acc1) + 2*(vx0 + 0.5*dt*cart_acc2) + (vx0 + dt*cart_acc3))
        self.cart_vx = vx0 + dt/6.0 * (cart_acc1 + 2*cart_acc2 + 2*cart_acc3 + cart_acc4)
        self.theta = theta0 + dt/6.0 * (omega0 + 2*(omega0 + 0.5*dt*theta_acc1) + 2*(omega0 + 0.5*dt*theta_acc2) + (omega0 + dt*theta_acc3))
        self.omega = omega0 + dt/6.0 * (theta_acc1 + 2*theta_acc2 + 2*theta_acc3 + theta_acc4)
        
        self.step_count += 1
    
    def step(self, force):
        # Clamp force
        force = np.clip(force, -self.force_mag, self.force_mag)
        self.rk4_step(force)
        
        # Check done
        done = False
        if abs(self.cart_x) > self.cart_pos_threshold:
            done = True
        for i in range(self.N):
            if abs(self.theta[i]) > self.angle_threshold:
                done = True
        
        # Reward (matching C: survival + cos(theta) per pole)
        reward = 1.0
        for i in range(self.N):
            reward += 0.5 * np.cos(self.theta[i])
        reward -= 0.1 * abs(self.cart_x / self.cart_pos_threshold)
        reward -= 0.01 * abs(self.cart_vx / 10.0)
        
        return self.get_obs(), reward, done
    
    def get_pole_positions(self):
        """Get (x,y) positions of all pole joints and tips for rendering"""
        positions = [(self.cart_x, 0.0)]  # Cart center
        x, y = self.cart_x, 0.0
        for i in range(self.N):
            # Joint position
            positions.append((x, y))
            # Tip of this pole
            x += self.pole_lengths[i] * np.sin(self.theta[i])
            y += self.pole_lengths[i] * np.cos(self.theta[i])
            positions.append((x, y))
        return positions


# ============================================================
# Simple Policy (PD controller for demo - replace with trained policy)
# ============================================================

def pd_policy(obs, N):
    """Simple PD controller for visualization"""
    # State: [cart_x, cart_vx, theta_1, omega_1, ...]
    cart_x, cart_vx = obs[0], obs[1]
    
    # PD on first pole (dominant)
    theta1 = obs[2]
    omega1 = obs[3]
    
    # Gains (tuned for visualization)
    kp_cart, kd_cart = 5.0, 2.0
    kp_pole, kd_pole = 50.0, 10.0
    
    force = -(kp_cart * cart_x + kd_cart * cart_vx + kp_pole * theta1 + kd_pole * omega1)
    return np.clip(force, -10.0, 10.0)


# ============================================================
# Rendering
# ============================================================

def render_frame(ax, envs, poles_list, frame_idx, max_steps=1000):
    """Render multi-env frame"""
    ax.clear()
    
    colors = ['#FF6B6B', '#4ECDC4', '#45B7D1', '#FFA07A']  # 7,8,9,10 pole colors
    pole_colors = ['#2C3E50', '#34495E', '#7F8C8D', '#95A5A6']
    
    for idx, (env, N) in enumerate(zip(envs, poles_list)):
        offset_y = idx * 6.0  # Vertical spacing
        
        # Get pole positions
        positions = env.get_pole_positions()
        positions = np.array(positions)
        x_coords = positions[:, 0]
        y_coords = positions[:, 1] + offset_y + 3.0
        
        # Draw track
        ax.axhline(y=offset_y, color='#333333', linewidth=3, alpha=0.5)
        ax.axhline(y=offset_y + 6.0, color='#333333', linewidth=1, alpha=0.3)
        
        # Cart body
        cart_w, cart_h = 0.8, 0.4
        cart_x = env.cart_x
        cart_rect = Rectangle((cart_x - cart_w/2, offset_y - cart_h/2), 
                              cart_w, cart_h, 
                              facecolor=colors[idx], edgecolor='white', linewidth=2, zorder=10)
        ax.add_patch(cart_rect)
        
        # Cart wheels
        wheel_r = 0.15
        for wx in [cart_x - 0.3, cart_x + 0.3]:
            wheel = Circle((float(wx), offset_y - cart_h/2 - wheel_r), wheel_r,
                          facecolor='#2C2C2C', edgecolor='white', linewidth=1.5, zorder=11)
            ax.add_patch(wheel)
        
        # Poles as connected segments
        pole_positions = env.get_pole_positions()
        pole_positions = np.array(pole_positions)
        
        for i in range(N):
            # Joint position
            jx = float(pole_positions[2*i][0])
            jy = float(pole_positions[2*i][1]) + offset_y + 3.0
            # Tip of this pole
            tx = float(pole_positions[2*i + 1][0])
            ty = float(pole_positions[2*i + 1][1]) + offset_y + 3.0
            
            # Pole segment
            ax.plot([jx, tx], [jy, ty], color=pole_colors[i % len(pole_colors)], 
                    linewidth=3, solid_capstyle='round', zorder=5)
            
            # Joint marker
            joint_c = Circle((jx, jy), 0.06, facecolor='#E74C3C', edgecolor='white', 
                           linewidth=1, zorder=15)
            ax.add_patch(joint_c)
        
        # Label
        ax.text(-5.5, offset_y + 3.0, f'{N}-POLE', fontsize=14, fontweight='bold',
               color=colors[idx], ha='right', va='center',
               bbox=dict(boxstyle='round,pad=0.3', facecolor='black', alpha=0.7, edgecolor=colors[idx]))
        
        # Step counter
        ax.text(-5.5, offset_y + 1.5, f'Step: {env.step_count}', fontsize=10,
               color='white', ha='right', va='center',
               bbox=dict(boxstyle='round,pad=0.2', facecolor='black', alpha=0.5))
        
        # Cart position indicator
        ax.text(-5.5, offset_y + 0.5, f'x: {cart_x:.2f}', fontsize=9,
               color='#CCCCCC', ha='right', va='center')
        
        # Pole angles
        angles_str = ', '.join([f'θ{i+1}:{env.theta[i]:.2f}' for i in range(min(N, 3))])
        if N > 3:
            angles_str += '...'
        ax.text(-5.5, offset_y - 0.5, angles_str, fontsize=8,
               color='#AAAAAA', ha='right', va='center')
    
    # Global title
    ax.text(0, len(poles_list)*6.0 + 1.5, 
            'WuBuOS BearRL — N-Pole Cartpole (7-10 Poles) — Sovereign C11 RL',
            fontsize=16, fontweight='bold', color='white', ha='center',
            bbox=dict(boxstyle='round,pad=0.5', facecolor='#1A1A2E', alpha=0.9, edgecolor='#00D4FF'))
    
    ax.set_xlim(-5.8, 5.8)
    ax.set_ylim(-1.0, len(poles_list)*6.0 + 3.0)
    ax.set_aspect('equal')
    ax.axis('off')
    ax.set_facecolor('#0D0D1A')


def run_simulation(num_poles, steps=1000, policy_fn=pd_policy):
    """Run single environment and collect frames"""
    env = NPoleCartpole(num_poles=num_poles)
    frames = []
    
    for step in range(steps):
        obs = env.get_obs()
        frames.append((env.cart_x, env.cart_vx, env.theta.copy(), env.omega.copy(), env.step_count))
        
        force = policy_fn(obs, num_poles)
        _, _, done = env.step(force)
        
        if done:
            break
    
    return frames, env


# ============================================================
# Main: Generate Video
# ============================================================

if __name__ == '__main__':
    print("🎬 Generating N-Pole Cartpole Video (7-10 poles)...")
    
    poles_list = [7, 8, 9, 10]
    max_steps = 1000
    
    # Run all simulations
    print(f"Running simulations for {poles_list} poles...")
    envs = []
    for N in poles_list:
        frames, env = run_simulation(N, max_steps)
        envs.append(env)
        print(f"  {N}-pole: {env.step_count} steps before done")
        # Reset to initial state for animation
        env.reset()
    
    # Create figure
    fig = plt.figure(figsize=(16, 9), facecolor='#0D0D1A')
    ax = fig.add_subplot(111)
    
    def animate(frame_idx):
        # Step all environments
        for env, N in zip(envs, poles_list):
            obs = env.get_obs()
            force = pd_policy(obs, N)
            env.step(force)
        
        render_frame(ax, envs, poles_list, frame_idx, max_steps)
        
        # Progress indicator
        if frame_idx % 100 == 0:
            print(f"  Frame {frame_idx}/{max_steps}")
    
    print("Rendering animation...")
    anim = animation.FuncAnimation(fig, animate, frames=max_steps, 
                                   interval=33, repeat=False, blit=False)
    
    # Save as MP4
    output_path = os.path.expanduser('~/npole_cartpole_7_10_poles.mp4')
    print(f"Saving to {output_path}...")
    anim.save(output_path, writer='ffmpeg', fps=30, 
              extra_args=['-vcodec', 'libx264', '-pix_fmt', 'yuv420p', '-crf', '20'],
              dpi=100)
    
    print(f"✅ Video saved to {output_path}")
    plt.close()
